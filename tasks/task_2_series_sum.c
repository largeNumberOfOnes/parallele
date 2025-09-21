/*
 * Finally, everyone is putting prime numbers together.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "mpi.h"

#include "../slibs/err_proc.h"



const int ZERO_TAG = 0;

double calc(int from, int to) {
    double sum = 0;
    for (int q = from; q < to; ++q) {
        sum += 1.0 / (q + 1);
    }
    return sum;
}

int main(int argc, char **argv) {

    RET_IF_ERR(MPI_Init(&argc, &argv));

    check(
        argc == 2,
        "Not enougth arguments: you need to specify N - summation number"
    );

    int size, rank;
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));

    int N = atoi(argv[1]);
    check(N != -1, "N is not an integer!");

    int from = N/size*rank;
    int to = (rank + 1 == size) ? (N) : (from + N/size);
    double sum = calc(from, to);

    printf("%d -> from: %d, to: %d\n", rank, from, to);
    printf("%d -> sum = %g\n", rank, sum);

    if (rank == 0) {
        for (int q = 1; q < size; ++q) {
            double other_sum = 0;
            RET_IF_ERR(
                MPI_Recv(
                    &other_sum, 1, MPI_DOUBLE, q,
                    ZERO_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE
                )
            );
            sum += other_sum;
        }
        printf("total sum: %g\n", sum);
    } else {
        RET_IF_ERR(
            MPI_Send(&sum, 1, MPI_DOUBLE, 0, ZERO_TAG, MPI_COMM_WORLD)
        );
    }

    RET_IF_ERR(MPI_Finalize());

    return 0;
}
