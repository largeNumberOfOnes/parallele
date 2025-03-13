#include <stdio.h>
#include "mpich/mpi.h"


#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


#define PRINT(...) {                                                      \
    printf("%d -> ", rank);                                               \
    printf(__VA_ARGS__);                                                  \
    printf("\n");                                                         \
}

#define PRINT_INT_ARR(arr) {                                              \
    printf("%d -> ", rank);                                               \
    for (int q = 0; q < (sizeof arr)/(sizeof arr[0]); ++q) {              \
        printf("%d ", arr[q]);                                            \
    }                                                                     \
    printf("\n");                                                         \
}

int main (int argc, char* argv[]) {

    int errCode;
    if (!(errCode = MPI_Init(&argc, &argv))) {
        return errCode;
    }

    int size, rank;
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // printf("size: %d!\n", size);
    // printf("rank: %d!\n", rank);

    const int SIMPLE_DATA = 0;
    if (rank == 0) {
        int buf[5] = {1, 2, 3, 4, 5};
        MPI_Send(buf, 3, MPI_INT, 1, SIMPLE_DATA, MPI_COMM_WORLD);
        PRINT_INT_ARR(buf);
    } else if (rank == 1) {
        int buf[5] = {0};
        MPI_Status status;
        PRINT_INT_ARR(buf);
        MPI_Recv(buf, 5, MPI_INT, 0, SIMPLE_DATA, MPI_COMM_WORLD, &status);
        PRINT_INT_ARR(buf);
    }



    MPI_Finalize();

    return 0;
}