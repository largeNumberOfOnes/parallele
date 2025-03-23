/*
 * Circular exile to Siberia.
 */

#include <stdio.h>
#include "mpi.h"

#include "../slibs/err_proc.h"



const int ZERO_TAG = 0;

int calc(int a) {
    return a + 7;
}

int main(int argc, char **argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    int size, rank;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    check(
        size > 1,
        "There is no point in throwing such a celebration on your own"
    );

    if (rank == 0) {
        int a = 0;
        RET_IF_ERR(
            MPI_Send(&a, 1, MPI_INT, 1, ZERO_TAG, MPI_COMM_WORLD)
        );
        RET_IF_ERR(
            MPI_Recv(
                &a, 1, MPI_INT, size - 1,
                ZERO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        int na = calc(a);
        printf("%d: %d -> %d\n", rank, a, na);
    } else {
        int a = 0;
        RET_IF_ERR(
            MPI_Recv(
                &a, 1, MPI_INT, rank - 1,
                ZERO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        int na = calc(a);
        printf("%d: %d -> %d\n", rank, a, na);
        RET_IF_ERR(
            MPI_Send(
                &na, 1, MPI_INT, (rank + 1) % size,
                ZERO_TAG, MPI_COMM_WORLD
            )
        );
    }

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
