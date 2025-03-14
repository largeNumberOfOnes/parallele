/**
 * This code outputs exit time statistics from MPI_Send, MPI_Ssend,
 *     MPI_Rsend and MPI_Bsend functions. The sender sends an array
 *     of sevens to the receiver and measures the time of function
 *     operation. The receiver waits 2 seconds after sending and
 *     receives the message. 
 * Important: It is possible that such behavior of the receiver may
 *     cause an error when executing the MPI_Rsend function.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "mpi.h"

#include "../slibs/err_proc.h"



const int ZERO_TAG = 0;
const int sleep_for = 2;
const int sender_rank   = 0;
const int receiver_rank = 1;

typedef enum Type_t {
    _SEND,
    SSEND,
    RSEND,
    BSEND,
} Type;

const char *typestr(Type type) {
    switch (type) {
        case _SEND: return "_SEND";
        case SSEND: return "SSEND";
        case RSEND: return "RSEND";
        case BSEND: return "BSEND";
        default:    return "UNKNOWN";
    }
}

int generalized_mpi_send(
    Type type, 
    const void* buf,
    int count,
    MPI_Datatype datatype,
    int dest,
    int tag,
    MPI_Comm comm
) {
    switch (type) {
        case _SEND:
            return MPI_Send(buf, count, datatype, dest, tag, comm);
        case SSEND:
            return MPI_Ssend(buf, count, datatype, dest, tag, comm);
        case RSEND:
            return MPI_Rsend(buf, count, datatype, dest, tag, comm);
        case BSEND:
            return MPI_Bsend(buf, count, datatype, dest, tag, comm);
        default:
            return MPI_ERR_NAME;
    }
}

int send_message(int buf_size, Type type, int receiver_rank) {
    int *buf = malloc(sizeof(int) * buf_size);
    RET_IF_ERR(!buf);
    for (int q = 0; q < buf_size; ++q) {
        buf[q] = 7;
    }

    clock_t start, end;

    start = clock();
    RET_IF_ERR(
        generalized_mpi_send(
            type,
            buf, buf_size, MPI_INT, receiver_rank,
            ZERO_TAG, MPI_COMM_WORLD
        )
    );
    end = clock();

    double delta_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("sender -> for %7d elements delta time: %f\n", 
                                                    buf_size, delta_time);
    
    free(buf);

    return 0;
}

int receive_message(int buf_size, int sender_rank) {
    int *buf = malloc(sizeof(int) * buf_size);

    sleep(sleep_for);
    RET_IF_ERR(
        MPI_Recv(
            buf, buf_size, MPI_INT, sender_rank,
            ZERO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
        )
    );
    
    for (int q = 0; q < buf_size; ++q) {
        if (buf[q] != 7) {
            printf("receiver -> Very bad array, litle berry!!!\n");
            break;
        }
    }

    free(buf);

    return 0;
}

int sender(
    const Type *types, int types_count,
    const int *sizes, int sizes_count,
    int receiver_rank
) {

    int buf_size = sizes[sizes_count-1]*sizeof(int) + MPI_BSEND_OVERHEAD;
    char *buf = malloc(buf_size);
    RET_IF_ERR(!buf);
    MPI_Buffer_attach(buf, buf_size);

    printf("sender -> Start test---------------------------------\n\n");
    for (int n_type = 0; n_type < types_count; ++n_type) {
        printf("sender -> type = %s\n", typestr(types[n_type]));
        for (int n_size = 0; n_size < sizes_count; ++n_size) {
            send_message(sizes[n_size], types[n_type], receiver_rank);
            MPI_Barrier(MPI_COMM_WORLD);
        }
        printf("\n");
    }

    MPI_Buffer_detach(buf, &buf_size);
    free(buf);
    
    return 0;
}

int receiver(
    const Type *types, int types_count,
    const int *sizes, int sizes_count,
    int sender_rank
) {

    for (int n_type = 0; n_type < types_count; ++n_type) {
        for (int n_size = 0; n_size < sizes_count; ++n_size) {
            receive_message(sizes[n_size], sender_rank);
            MPI_Barrier(MPI_COMM_WORLD);
        }
    }

    return 0;
}



int main (int argc, char* argv[]) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    int rank = 0, size = 0;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    if (size != 2) { // There should be only two processes
        if (rank == 0) {
            printf("You must specify '-n 2' for this program!\n");
        }
        RET_IF_ERR(1);
    }

    const Type types[] = {_SEND, SSEND, RSEND, BSEND};
    int types_count = sizeof(types) / sizeof(types[0]);
    const int sizes[] = {10, 100, 1000, 1000*10, 1000*100, 1000*1000,
                                            1000*1000*10, 1000*1000*100};
    int sizes_count = sizeof(sizes) / sizeof(sizes[0]);

    if (rank == sender_rank) {
        sender(types, types_count, sizes, sizes_count, receiver_rank);
    } else if (rank == receiver_rank) {
        receiver(types, types_count, sizes, sizes_count, sender_rank);
    }

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
