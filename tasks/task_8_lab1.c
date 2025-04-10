#include <assert.h>
#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
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
        }
        fprintf(file, "\n");
    }
}

// ------------------------------------------------------------------------

void calc_proc_problem(Problem *problem, int rank, int size) {
    problem->nx /= size;
    double range_len = (problem->x1 - problem->x0);
    problem->x0 += rank*range_len/size;
    problem->x1  = problem->x0 + range_len/size;
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

void calc_first_layer(Matrix *matrix, Problem *problem) {
    for (int q = 0; q < problem->nx; ++q) {
        matrix_set_elem(
            matrix, 0, q,
            problem->u0(problem->x0 + q*(problem->tau))
        );
    }
}

Matrix gather_matrixes(Matrix *matrix, int rank, int size) {
    Matrix buf = matrix_init(matrix->nx *size, matrix->nt);
    Matrix result = matrix_init(matrix->nx *size, matrix->nt);

    RET_IF_ERR(
        MPI_Gather(
            matrix->arr, matrix->size, MPI_DOUBLE, 
            buf.arr, matrix->size, MPI_DOUBLE, 
            main_rank, MPI_COMM_WORLD
        )
    );

    for (int proc = 0; proc < size; ++proc) {
        for (int time_layer = 0; time_layer < matrix->nt; ++time_layer) {
            memcpy(
                result.arr + proc*matrix->nx + result.nx*time_layer,
                buf.arr + proc*matrix->size + time_layer*matrix->nx,
                sizeof(double) * matrix->nx
            );
        }
    }

    return result;
}

void calc_layer(
    Problem *problem,
    double *new_layer,
    double *prev_layer,
    double t
) {
    for (int x_pos = 1; x_pos < problem->nx; ++x_pos) {
        new_layer[x_pos] = calc_next_elem(
            new_layer[x_pos - 1], 
            prev_layer[x_pos - 1], 
            prev_layer[x_pos], 
            problem->a,
            problem->f(
                problem->x0 + x_pos*problem->h,
                t
            ),
            problem->tau, problem->h
        );
    }
}

void send_edge(
    double *buf,
    int buf_w,
    int buf_h,
    double *edge_buf,
    int rank,
    int size
) {
    if (rank + 1 != size) {
        for (int q = 0; q < buf_h; ++q) {
            edge_buf[q] = buf[buf_w*(q+1) - 1];
        }
        RET_IF_ERR(
            MPI_Send(
                edge_buf, buf_h, MPI_DOUBLE, 
                rank + 1, TAG_EDGE, 
                MPI_COMM_WORLD
            )
        );
    }
}

void recv_edge(
    double *buf,
    int buf_w,
    int buf_h,
    double *edge_buf,
    Problem *problem,
    double buf_start_t,
    int rank,
    int size
) {
    if (rank == main_rank) {
        for (int q = 0; q < buf_h; ++q) {
            buf[q*buf_w] = problem->u1(buf_start_t + q*problem->tau);
        }
    } else {
        RET_IF_ERR(
            MPI_Recv(
                edge_buf, buf_h, MPI_DOUBLE, 
                rank - 1, TAG_EDGE,
                MPI_COMM_WORLD, MPI_STATUS_IGNORE
            )
        );
        for (int q = 0; q < buf_h; ++q) {
            buf[q*buf_w] = edge_buf[q];
        }
    }
}

void calc_batch(
    double *buf,
    int buf_w,
    int buf_h,
    Problem *problem,
    double buf_start_t,
    int calc_next_layer
) {
    // int calc_next_layer = buffer_number + 1 != problem.nt / batch_size;
    for (int time_layer = 1; time_layer < buf_h + calc_next_layer; ++time_layer) {
        calc_layer(
            problem, 
            buf + time_layer * buf_w,
            buf + (time_layer - 1) * buf_w,
            buf_start_t + time_layer*problem->tau
        );
    }
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


    const int batch_size = 10;
    assert(problem.nt % batch_size == 0);

    double *edje_buf = (double*) malloc(batch_size * sizeof(double));
    for (int buffer_number = 0; buffer_number < problem.nt / batch_size; ++buffer_number) {
        int buffer_size = matrix.nx * batch_size;
        double *buffer = matrix.arr + buffer_number * buffer_size;

        recv_edge(
            buffer, matrix.nx, batch_size,
            edje_buf,
            &problem, problem.t0 + (buffer_number*batch_size)*problem.tau,
            rank, size
        );
        calc_batch(
            buffer, matrix.nx, batch_size, 
            &problem,
            problem.t0 + buffer_number*batch_size*problem.tau,
            buffer_number + 1 != problem.nt / batch_size
        );
        send_edge(buffer, matrix.nx, batch_size, edje_buf, rank, size);
    }
    free(edje_buf);

    // sync_call(
    //     printf("rank %d\n", rank);
    //     for (int time_layer = 0; time_layer < problem.nt; ++time_layer) {
    //         print_arr(matrix.arr + time_layer*matrix.nx, matrix.nx);
    //     }
    //     printf("\n");
    // )

    // Matrix result = matrix;
    Matrix result = gather_matrixes(&matrix, rank, size);
    if (rank == main_rank) {
        FILE *file = fopen("output/calc.txt", "w");
        matrix_dump(&result, file);
        fclose(file);
    }
    matrix_dstr(&result);

    matrix_dstr(&matrix);
}

// ------------------------------------------------------------------- main

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
