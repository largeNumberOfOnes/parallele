/**
 * Solving the transfer equation using the rectangle method.
 */

#include <mpi.h>
#include <time.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../slibs/err_proc.h"

// ------------------------------------------------------------------------

const int main_rank = 0;
const int TAG_EDGE  = 1;

// ------------------------------------------------------------------------

void print_arr(double *arr, int count) {
    check(arr);
    check(count > 0);

    for (int q = 0; q < count - 1; ++q) {
        printf("%f, ", arr[q]);
    }
    printf("%f\n", arr[count - 1]);
}

// --------------------------------------------------------- Problem struct

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

// ---------------------------------------------------------- Matrix struct

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

void matrix_dump(Matrix *matrix, FILE *file) {
    for (int q = 0; q < matrix->nt; ++q) {
        for (int w = 0; w < matrix->nx; ++w) {
            fprintf(file, "%f, ", matrix->arr[q*matrix->nx + w]);
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
        matrix->arr[q] = problem->u0(problem->x0 + q*problem->tau);
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
    for (int tlayer = 1; tlayer < buf_h + calc_next_layer; ++tlayer) {
        calc_layer(
            problem, 
            buf + tlayer * buf_w,
            buf + (tlayer - 1) * buf_w,
            buf_start_t + tlayer*problem->tau
        );
    }
}

void calc_local_problem(
    Matrix *matrix,
    Problem *problem,
    int rank,
    int size
) {
    int nt = problem->nt;
    int nx = problem->nx;

    const int batch_size = 10;
    check(nt % batch_size == 0);

    double *edje_buf = (double*) malloc(batch_size * sizeof(double));
    for (int buffer_num = 0; buffer_num < nt / batch_size; ++buffer_num) {
        int buffer_size = nx * batch_size;
        double *buffer = matrix->arr + buffer_num * buffer_size;
        double buf_start_t = problem->t0 +
                                        buffer_num*batch_size*problem->tau;
        recv_edge(
            buffer, nx, batch_size,
            edje_buf,
            problem, buf_start_t,
            rank, size
        );
        calc_batch(
            buffer, nx, batch_size, 
            problem,
            buf_start_t,
            buffer_num + 1 != nt / batch_size
        );
        send_edge(buffer, nx, batch_size, edje_buf, rank, size);
    }
    free(edje_buf);
}

Matrix calc(
    Problem problem,
    int rank,
    int size
) {
    check(problem.nx % size == 0);
    calc_proc_problem(&problem, rank, size);
    Matrix matrix = matrix_init(problem.nx, problem.nt);

    calc_first_layer(&matrix, &problem);
    calc_local_problem(&matrix, &problem, rank, size);
    Matrix result = gather_matrixes(&matrix, rank, size);
    
    matrix_dstr(&matrix);

    return result;
}

// ------------------------------------------------------------------- main

double f(double x, double t) {
    return 0;
}

double u0(double x) {
    if (M_PI / 9 < x && x < M_PI / 6) {
        return 10*sin(18*x);
    } else {
        return 0;
    }
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
        0, 1, 1000,
        0, 1, 1000,
        1, f, u0, u1
    );

    clock_t start, end;
    start = clock();
    Matrix result = calc(problem, rank, size);
    end = clock();
    double delta_time = (double)(end - start) / CLOCKS_PER_SEC;
    rprint("time spent: %f", delta_time);

    if (rank == main_rank) {
        FILE *file = fopen("output/calc.txt", "w");
        matrix_dump(&result, file);
        fclose(file);
    }
    matrix_dstr(&result);

    MPI_Finalize();

    return 0;
}
