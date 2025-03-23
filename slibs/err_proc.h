#pragma once

#if defined(MPI_VERSION) || defined(OMPI_CC) || defined(MPICH_CC)
    #define IT_IS_MPICOMP
#endif

#ifdef IT_IS_MPICOMP
    #include "mpi.h"
#endif

#ifdef IT_IS_MPICOMP
    #define RET_IF_ERR_EXIT_WAY MPI_Abort(MPI_COMM_WORLD, -1); return -1
#else
    #define RET_IF_ERR_EXIT_WAY return -1
#endif


#ifdef __cplusplus
    #include <iostream>
    #include <cstdio>
#else
    #include <stdio.h>
#endif

#define RET_IF_ERR(expr) {                                                \
    if (expr) {                                                           \
        printf("\033[91mSome problem: %d\033[39m\n", __LINE__);           \
        fflush(stdout);                                                   \
        RET_IF_ERR_EXIT_WAY;                                              \
    }                                                                     \
}

#ifdef IT_IS_MPICOMP
    #define rprint(...) {                                                 \
        int rank = 0;                                                     \
        RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));                 \
        printf("%d -> ", rank);                                           \
        printf(__VA_ARGS__);                                              \
        printf("\n");                                                     \
    }
#else
    #define rprint(...) {                                                 \
        printf("0 -> ");                                                  \
        printf(__VA_ARGS__);                                              \
        printf("\n");                                                     \
    }
#endif

#ifdef IT_IS_MPICOMP
    #define check(cond, ...) {                                            \
        if (!(cond)) {                                                    \
            int rank = 0;                                                 \
            RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));             \
            if (rank == 0) {                                              \
                fprintf(stderr, "\033[91mError: ");                       \
                fprintf(stderr, __VA_ARGS__);                             \
                fprintf(                                                  \
                    stderr, "\nProblem in line %d: %s\033[39m\n",         \
                    __LINE__, #cond                                       \
                );                                                        \
            }                                                             \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
#else
    #define check(cond, ...) {                                            \
        if (!(cond)) {                                                    \
            fprintf(stderr, "\033[91mError: ");                           \
            fprintf(stderr, __VA_ARGS__);                                 \
            fprintf(                                                      \
                stderr, "\nProblem in line %d: %s\033[39m\n",             \
                __LINE__, #cond                                           \
            );                                                            \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
#endif


// Destroing internal macro words
#undef IT_IS_MPICOMP
