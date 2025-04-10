#include <mpi.h>
#include <stdlib.h>
#include <time.h>
#include "../slibs/err_proc.h"



const int TAG = 0;

static void sender_activity(int *buf, int count) {
    RET_IF_ERR(
        MPI_Send(
            buf, count, MPI_INT, 1, TAG,
            MPI_COMM_WORLD
        )
    );
}

static void receiver_activity(int *buf, int count) {
    RET_IF_ERR(
        MPI_Recv(
            buf, count, MPI_INT, 0,
            TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
        )
    );
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    check(size == 2)
    check(argc == 2);
    int count = atoi(argv[1]);
    check(count > 0);

    int *buf = (int*) malloc(count * sizeof(int));
    clock_t start, end;
    start = clock();
    if (rank == 0) {
        sender_activity(buf, count);
    } else {
        receiver_activity(buf, count);
    }
    end = clock();
    double delta = (double)(end - start) / CLOCKS_PER_SEC;
    rprint("time spent[%s]: %f", rank ? "receiver" : "sender", delta);
    free(buf);

    MPI_Finalize();

    return 0;
}
