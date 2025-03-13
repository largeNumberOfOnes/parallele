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
    #define RET_IF_ERR(expr) {                                            \
        if (expr) {                                                       \
            std::cout << "\033[91mSome problem: "                         \
                                << __LINE__ << "\033[39m" << std::endl;   \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
#else
    #include <stdio.h>
    #define RET_IF_ERR(expr) {                                            \
        if (expr) {                                                       \
            printf("\033[91mSome problem: %d\033[39m\n", __LINE__);       \
            fflush(stdout);                                               \
            RET_IF_ERR_EXIT_WAY;                                          \
        }                                                                 \
    }
#endif

// #define PRINT(...) {                                                      \
//     printf("%d -> ", rank);                                               \
//     printf(__VA_ARGS__);                                                  \
//     printf("\n");                                                         \
// }

// #define PRINT_INT_ARR(arr) {                                              \
//     printf("%d -> ", rank);                                               \
//     for (int q = 0; q < (sizeof arr)/(sizeof arr[0]); ++q) {              \
//         printf("%d ", arr[q]);                                            \
//     }                                                                     \
//     printf("\n");                                                         \
// }

// Destroing internal macro words
#undef IT_IS_MPICOMP
