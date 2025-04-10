#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include "math.h"
#include "../slibs/err_proc.h"



const int main_rank = 0;
const int TAG_EDGE  = 1;

// ---------------------------------------------------------- Proble struct

void print_arr(double *arr, int count) {
    check(arr);
    check(count > 0);

    for (int q = 0; q < count - 1; ++q) {
        printf("%f, ", arr[q]);
    }
    printf("%f\n", arr[count - 1]);
}

// ---------------------------------------------------------- Proble struct

typedef struct Problem_t {
    double x0; double x1; int nx;
    double t0; double t1; int nt;
    double a;
    double (*f) (double x, double t);
    double (*u0) (double x);
    double (*u1) (double t);
    double tau;
    double h;
} Problem;

Problem problem_init(
    double x0, double x1, int nx,
    double t0, double t1, int nt,
    double a,
    double (*f) (double x, double t),
    double (*u0) (double x),
    double (*u1) (double t)
) {
    return (Problem) {
        .x0 = x0,
        .x1 = x1,
        .nx = nx,
        .t0 = t0,
        .t1 = t1,
        .nt = nt,
        .a  = a,
        .f  = f,
        .u0 = u0,
        .u1 = u1,
        .tau = (t1 - t0) / (nt - 1),
        .h   = (x1 - x0) / (nx - 1)
    };
}

void problem_dump(Problem *problem) {
    printf(
        "x: %f - %f; points: %d; h %f \n",
        problem->x0,
        problem->x1,
        problem->nx,
        problem->h
    );
    printf(
        "t: %f - %f; points: %d; tau: %f\n",
        problem->t0,
        problem->t1,
        problem->nt,
        problem->tau
    );    
}

// ------------------------------------------------------------------------

typedef struct Matrix_t {
    double *arr;
    int size;
    int nx;
    int nt;
} Matrix;

Matrix matrix_init(int nx, int nt) {
    return (Matrix) {
        .arr = (double*) malloc(nt * nx * sizeof(double)),
        .size = nx * nt,
        .nx = nx,
        .nt = nt,
    };
}

void matrix_dstr(Matrix *matrix) {
    free(matrix->arr);
    matrix->arr = NULL;
}

static inline double matrix_get_elem(Matrix *matrix, int layer, int elem_number) {
    return matrix->arr[layer*(matrix->nx) + elem_number];
}

static inline void matrix_set_elem(Matrix *matrix, int layer, int elem_number,
                                                            double elem) {
    matrix->arr[layer*(matrix->nx) + elem_number] = elem;
}

void matrix_dump(Matrix *matrix, FILE *file) {
    printf("nt: %d\n", matrix->nt);
    for (int q = 0; q < matrix->nt; ++q) {
        for (int w = 0; w < matrix->nx; ++w) {
            fprintf(file, "%f, ", matrix_get_elem(matrix, q, w));
            // printf("%f, ", matrix_get_elem(matrix, q, w));
        }
        fprintf(file, "\n");
    }
}

// ------------------------------------------------------------------------

// void calc_proc_problem(double *x0, double *x1, int *nx, int rank, int size) {
void calc_proc_problem(Problem *problem, int rank, int size) {
    problem->nx /= size;
    double range_len = (problem->x1 - problem->x0);
    problem->x0 += rank*range_len/size;
    problem->x1  = problem->x0 + range_len/size;
}

void get_edje(
    Matrix *matrix,
    Problem *problem,
    int edje_size,
    int from,
    int rank,
    int size
    // int *ret_arr
) {
    if (rank == main_rank) {
        double tau = problem->tau;
        for (int q = 0; q < edje_size; ++q) {
            // ret_arr[q] = problem->u1(problem->t0 + q*tau);
            matrix_set_elem(matrix, from + q, 0, problem->u1(problem->t0 + q*tau));
        }
    } else {
        double *buf = (double*) malloc(edje_size * sizeof(double));
        RET_IF_ERR(
            MPI_Recv(
                buf, edje_size, MPI_DOUBLE,
                rank - 1, TAG_EDGE,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        for (int q = 0; q < edje_size; ++q) {
            matrix_set_elem(matrix, from + q, 0, buf[q]);
        }
        free(buf);
    }
}

static inline double calc_next_elem(
    double left_up,
    double left_down,
    double right_down,
    double a,
    double f,
    double tau,
    double h
) {
    return (f - (left_up - left_down - right_down)/2/tau
        - a*(right_down - left_up - left_down)/2/h)/(0.5/tau + a*0.5/h);
}


void calc_layer(Matrix *matrix, Problem *problem, int layer_number) {
    for (int q = 1; q < problem->nx; ++q) {
        calc_next_elem(
            matrix_get_elem(matrix, layer_number    , q - 1),
            matrix_get_elem(matrix, layer_number - 1, q - 1),
            matrix_get_elem(matrix, layer_number - 1, q    ),
            problem->a,
            problem->f(
                problem->x0 + (problem->h  )*(q + 0.5),
                problem->t0 + (problem->tau)*(layer_number - 0.5)
            ),
            problem->tau,
            problem->h
        );
    }
}

void calc_first_layer(Matrix *matrix, Problem *problem) {
    for (int q = 0; q < problem->nx; ++q) {
        matrix_set_elem(
            matrix, 0, q,
            problem->u0(problem->x0 + q*(problem->tau))
        );
    }
}

void calc_batch(Matrix *matrix, Problem *problem, int from, int batch_size, int rank, int size) {
    get_edje(matrix, problem, batch_size, from, rank, size);
    for (int q = 0; q < batch_size; ++q) {
        calc_layer(matrix, problem, from + q);
        print_arr(matrix->arr + (from + q)*matrix->nx, matrix->nx);
    }
    printf("\n\n");
    for (int q = 0; q < batch_size; ++q) { printf("%d ", q); print_arr(matrix->arr + q*matrix->nx, matrix->nx); }
}

void send_edge(
    Matrix *matrix,
    int edje_size,
    int edje_number,
    int rank,
    int size
) {
    double *buf = (double*) malloc(edje_size * sizeof(double));
    for (int q = 0; q < edje_size; ++q) {
        buf[q] = matrix_get_elem(
            matrix,
            edje_number*edje_size + q,
            matrix->nx - 1
        );
    }
    if (rank + 1 < size) {
        RET_IF_ERR(
            MPI_Send(
                buf, edje_size, MPI_DOUBLE,
                rank + 1, TAG_EDGE, MPI_COMM_WORLD
            )
        );
    }
    free(buf);
}

Matrix gather_matrixes(Matrix *matrix, int rank, int size) {
    Matrix result = matrix_init(matrix->nx *size, matrix->nt);

    RET_IF_ERR(
        MPI_Gather(
            matrix->arr, matrix->size, MPI_DOUBLE, 
            result.arr, matrix->size, MPI_DOUBLE, 
            main_rank, MPI_COMM_WORLD
        )
    );

    return result;
}

void calc(
    Problem problem,
    int rank,
    int size
) {
    // calc_x_range(&x0, &x1, &nx, rank, size);
    calc_proc_problem(&problem, rank, size);
    // rprint("x0 = %f, x1 = %f, nx = %d", x0, x1, nx);
    LOG(problem_dump(&problem));

    Matrix matrix = matrix_init(problem.nx, problem.nt);

    calc_first_layer(&matrix, &problem);

    LOG(
        printf("First layer\n");
        print_arr(matrix.arr, matrix.nx);
    )
    
    for (int time_layer = 1; time_layer < problem.nt; ++time_layer) {
        double *new_layer  = matrix.arr + time_layer * matrix.nx;
        double *prev_layer = matrix.arr + (time_layer - 1) * matrix.nx;

        for (int x_pos = 1; x_pos < problem.nx; ++x_pos) {
            if (rank == main_rank) {
                new_layer[0] = problem.u1(problem.x0 + problem.tau * time_layer);
            } else {
                RET_IF_ERR(
                    MPI_Recv(
                        new_layer, 1, MPI_DOUBLE, 
                        rank - 1, TAG_EDGE,
                        MPI_COMM_WORLD, MPI_STATUS_IGNORE
                    )
                );
            }
            new_layer[x_pos] = calc_next_elem(
                new_layer[x_pos - 1], 
                prev_layer[x_pos - 1], 
                prev_layer[x_pos], 
                problem.a,
                problem.f(
                    problem.x0 + x_pos*problem.h,
                    problem.t0 + time_layer*problem.tau
                ), 
                problem.tau, problem.h
            );
            if (rank + 1 != size) {
                RET_IF_ERR(
                    MPI_Send(
                        new_layer + problem.nx - 1, 1, MPI_DOUBLE, 
                        rank + 1, TAG_EDGE, 
                        MPI_COMM_WORLD
                    )
                );
            }
        }
    }

    // sync_call(
    //     printf("rank %d\n", rank);
    //     for (int time_layer = 0; time_layer < problem.nt; ++time_layer) {
    //         print_arr(matrix.arr + time_layer*matrix.nx, matrix.nx);
    //     }
    //     printf("\n");
    // )

    Matrix result = matrix;
    // Matrix result = gather_matrixes(&matrix, rank, size);
    if (rank == main_rank) {
        FILE *file = fopen("output/calc.txt", "w");
        matrix_dump(&result, file);
        fclose(file);
    }
    // matrix_dstr(&result);

    matrix_dstr(&matrix);
}

double f(double x, double t) {
    return 0;
}

double u0(double x) {
    return 10*sin(18*x);
}

double u1(double t) {
    return 0;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank = 0, size = 0;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));

    Problem problem = problem_init(
        0, 1, 100,
        0, 1, 100,
        1, f, u0, u1
    );
    check(problem.nx % size == 0);

    calc(
        problem,
        rank, size
    );

    MPI_Finalize();

    return 0;
}
