/**
 * My first MPI program.
 */

#include <stdio.h>
#include "mpi.h"

#include "../slibs/err_proc.h"

int main(int argc, char **argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    int size, rank;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    printf("Hello world!\n");
    printf("Size: %d\n", size);
    printf("Rank: %d\n", rank);

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
