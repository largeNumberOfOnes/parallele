/**
 * Measurement of the average time to forward a message of some length.
 */

#include <mpi.h>
#include <stdlib.h>
#include <time.h>
#include "../slibs/err_proc.h"
#include "mpi_proto.h"



const int TAG = 0;
const int loop_count = 1000;
const int main_rank = 0;

static void sender_activity(int *buf, int count) {
    for (int q = 0; q < loop_count; ++q) {
        RET_IF_ERR(
            MPI_Send(
                buf, count, MPI_INT, 1, TAG,
                MPI_COMM_WORLD
            )
        );
    }
}

static void receiver_activity(int *buf, int count) {
    for (int q = 0; q < loop_count; ++q) {
        RET_IF_ERR(
            MPI_Recv(
                buf, count, MPI_INT, 0,
                TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
    }
}

static void measure_time(
    double *start_time,
    double *end_time,
    int count,
    int rank,
    int size
) {
    int *buf = (int*) malloc(count * sizeof(int));
    clock_t start, end;
    start = clock();
    if (rank == 0) {
        sender_activity(buf, count);
    } else {
        receiver_activity(buf, count);
    }
    end = clock();
    free(buf);

    *start_time = (double)start / CLOCKS_PER_SEC;
    *end_time   = (double)end / CLOCKS_PER_SEC;
}

static void gather_time(double *time_arr, double *recv_arr) {
    RET_IF_ERR(
        MPI_Gather(
            time_arr, 2, MPI_DOUBLE, 
            recv_arr, 2, MPI_DOUBLE, 
            main_rank, MPI_COMM_WORLD
        )
    );
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    check(size == 2);
    check(argc == 2);
    int count = atoi(argv[1]);
    check(count > 0);

    double time_arr[2];
    measure_time(time_arr, time_arr + 1, count, rank, size);

    double recv_arr[4];
    gather_time(time_arr, recv_arr);

    if (rank == main_rank) {
        double sender_start_time = recv_arr[0];
        double receiver_end_time = recv_arr[3];
        printf(
            "average time spent: %f\n",
            (receiver_end_time - sender_start_time) / loop_count
        );
    }

    MPI_Finalize();

    return 0;
}
