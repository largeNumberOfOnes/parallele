#pragma once

#include <stdint.h>
#if defined(MPI_VERSION) || defined(OMPI_CC) || defined(MPICH_CC)
    #define IT_IS_MPICOMP
    #include "mpi.h"
#endif


#ifdef IT_IS_MPICOMP
    #define RET_IF_ERR_EXIT_WAY MPI_Abort(MPI_COMM_WORLD, -1); abort();
#else
    #define RET_IF_ERR_EXIT_WAY abort()
#endif


#ifdef __cplusplus
    #include <iostream>
    #include <cstdlib>
    #include <cstdio>
#else
    #include <stdlib.h>
    #include <stdio.h>
#endif


#define RET_IF_ERR(expr) {                                                \
    if (expr) {                                                           \
        fprintf(stderr, "\033[91mSome problem: %d\033[39m\n", __LINE__);  \
        fflush(stdout);                                                   \
        RET_IF_ERR_EXIT_WAY;                                              \
    }                                                                     \
}


#ifdef IT_IS_MPICOMP
    #define check_ames(cond, mes) {                                       \
        if (!(cond)) {                                                    \
            int rank = 0;                                                 \
            RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));             \
            if (rank == 0) {                                              \
                fprintf(                                                  \
                    stderr,                                               \
                    "\033[91mError: condition %s in line %d is not "      \
                                                        "met\n",          \
                    #cond,                                                \
                    __LINE__                                              \
                );                                                        \
                fprintf(stderr, " |   %s\033[39m\n", mes);                \
                fflush(stdout);                                           \
            }                                                             \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
    #define check(cond) check_ames(cond, "Exceptional situation, however");
#else
    #define check_ames(cond, mes) {                                       \
        if (!(cond)) {                                                    \
            fprintf(                                                      \
                stderr,                                                   \
                "\033[91mError: condition %s in line %d is not "          \
                                                    "met\n",              \
                #cond,                                                    \
                __LINE__                                                  \
            );                                                            \
            fprintf(stderr, " |   %s\033[39m\n", mes);                    \
            fflush(stdout);                                               \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
    #define check(cond) check_ames(cond, "Exceptional situation, however");
#endif


#ifdef IT_IS_MPICOMP
    #define sync_call(call) {                                             \
        int ____rank = 0, ____size = 0;                                   \
        RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &____rank));             \
        RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &____size));             \
        for (int ____q = 0; ____q < ____size+1; ++____q) {                \
            MPI_Barrier(MPI_COMM_WORLD);                                  \
            if (____q == ____rank) {                                      \
                call;                                                     \
            }                                                             \
        }                                                                 \
    }
#else
    #define sync_call(call) {                                             \
        call;                                                             \
    }
#endif

#ifdef IT_IS_MPICOMP
    #define rprint(...) {                                                 \
        int rank = 0;                                                     \
        RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));                 \
        sync_call({                                                       \
            printf("%d -> ", rank);                                       \
            printf(__VA_ARGS__);                                          \
            printf("\n");                                                 \
            fflush(stdout);                                               \
        });                                                               \
    }
#else
    #define rprint(...) {                                                 \
        printf("0 -> ");                                                  \
        printf(__VA_ARGS__);                                              \
        printf("\n");                                                     \
        fflush(stdout);                                                   \
    }
#endif

static inline void rprint_arr(int *arr, int count) {
    check(arr);

    char *str = (char*) malloc(9 * (unsigned int)count * sizeof(char));
    int offset = 0;
    for (int q = 0; q < count; ++q) {
        offset += sprintf(str + offset, "%d, ", arr[q]);
    }
    str[offset] = '\0';
    rprint("%s", str);
    free(str);
}

#define DOT printf("\033[95mDOT from line: %d\033[39m\n", __LINE__);
#define DOTR {                                                            \
    MPI_Barrier(MPI_COMM_WORLD);                                          \
    printf("\033[95mDOT %d from line: %d\033[39m\n", rank, __LINE__);     \
    fflush(stdout);                                                       \
}

// Destroing internal macro words
#undef IT_IS_MPICOMP
