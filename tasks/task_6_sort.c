/**
 * RUN: ./prog <mode> <count_per_proc>
 * mode - One of the following:
 *        1 - run heapsort tim(N) dependence
 *        2 - run quicksort tim(N) dependence
 *        3 - run samplesort tim(N) dependence
 *        4 - run heapsort test
 *        5 - run quicksort test
 *        6 - run samplesort test
 */

#include "mpi.h"
#include "../slibs/err_proc.h"
#include <limits.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>



// ------------------------------------------------------- Global variables

const int main_rank = 0;

// ------------------------------------ Nice things for working with arrays

void print_arr(int *arr, int count) {
    check(arr);
    check(count > 0);

    for (int q = 0; q < count - 1; ++q) {
        printf("%d, ", arr[q]);
    }
    printf("%d\n", arr[count - 1]);
}

int check_arr(int *arr, int count) {
    check(arr);

    int correct = 1;
    for (int q = 1; q < count; ++q) {
        if (arr[q] < arr[q-1]) {
            correct = 0;
            return 0;
        }
    }
    return 1;
}

int *generate_random_array(int count, int max, int seed) {
    srand(seed);
    int *arr = (int*) malloc(count*sizeof(int));
    for (int q = 0; q < count; ++q) {
        arr[q] = rand() % max;
    }
    return arr;
}

// --------------------------------------------------------------- heapsort
// Standard heapsort realization

void heapify(int *arr, int n, int i) {
    check(arr);
    check(n > 0);

    int largest = i;
    int left  = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < n && arr[left] > arr[largest])
        largest = left;

    if (right < n && arr[right] > arr[largest])
        largest = right;

    if (largest != i) {
        int tmp = arr[largest];
        arr[largest] = arr[i];
        arr[i] = tmp;
        heapify(arr, n, largest);
    }
}

void heapsort(int *arr, int n) {
    for (int i = n / 2 - 1; i > -1; --i)
        heapify(arr, n, i);

    for (int i = n - 1; i > 0; --i) {
        int tmp = arr[0];
        arr[0] = arr[i];
        arr[i] = tmp;
        heapify(arr, i, 0);
    }
}

// -------------------------------------------------------------- quicksort
// Standard quicksort realization

void swap_int(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

static int partition(int *arr, int l, int r) {
    int v = arr[(l + r) / 2];
    int i = l;
    int j = r;
    while (i <= j) {
        while (arr[i] < v) {
           i++;
        }
        while (arr[j] > v) {
           j--;
        }
        if (i >= j) {
           break;
        }
        swap_int(&arr[i++], &arr[j--]);
    }
    return j;
}

void quicksort(int *arr, int l, int r) {
    if (l < r) {
        int q = partition(arr, l, r);
        quicksort(arr, l, q);
        quicksort(arr, q + 1, r);
    }
}

// ------------------------------------------------------------- radix_sort

int get_max(int *arr, int count) {
    int max = arr[0];
    for (int q = 1; q < count; ++q) {
        if (arr[q] > max) {
            max = arr[q];
        }
    }
    return max;
}

// Функция Counting Sort для сортировки по разряду (exp)
void countingSort(int arr[], int n, int exp) {
    // int output[n]; // Выходной массив
    // int *output = (int*) calloc(n, sizeof(int));
    int output[1000];
    int count[10] = {0}; // Счётчик цифр (0-9)

    // Подсчёт количества вхождений каждой цифры
    for (int i = 0; i < n; i++) {
        int digit = (arr[i] / exp) % 10;
        count[digit]++;
    }

    // Накопление суммы для определения позиций
    for (int i = 1; i < 10; i++) {
        count[i] += count[i - 1];
    }

    // Построение выходного массива
    for (int i = n - 1; i >= 0; i--) {
        int digit = (arr[i] / exp) % 10;
        output[count[digit] - 1] = arr[i];
        count[digit]--;
    }

    // Копирование отсортированного массива в исходный
    for (int i = 0; i < n; i++) {
        arr[i] = output[i];
    }
    // free(output);
}

// Основная функция Radix Sort
void radixsort(int arr[], int n) {
    int max = get_max(arr, n);

    // Применяем Counting Sort для каждого разряда
    for (int exp = 1; max / exp > 0; exp *= 10) {
        countingSort(arr, n, exp);
    }
}

// ------------------------------------------------------------ sample_sort

static void calc_splitters(
    int *self_arr,
    int *splitters,
    int count,
    int rank,
    int size
) {
    int splitters_count = size;
    for (int q = 0; q < size; ++q) {
        splitters[q] = self_arr[q*count/size];
    }

    int all_splitters_count = splitters_count * size;
    int *all_splitters = NULL;
    if (rank == main_rank) {
        all_splitters = (int*) malloc(all_splitters_count*sizeof(int));
    }

    RET_IF_ERR(
        MPI_Gather(
            splitters, splitters_count, MPI_INT, 
            all_splitters, splitters_count, MPI_INT, 
            main_rank, MPI_COMM_WORLD
        )
    )

    if (rank == main_rank) {
        quicksort(all_splitters, 0, all_splitters_count - 1);
        int offset = size/2 - 1;
        for (int q = 1; q < size; ++q) {
            splitters[q-1] = all_splitters[q*size + offset];
        }
        free(all_splitters);
    }

    RET_IF_ERR(
        MPI_Bcast(
            splitters, splitters_count-1, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );
}

static void separate_elements(
    int *buf, int buf_size,
    int backet_size,
    int backets_count,
    int *count_arr,
    int *splitters
) {
    int backet_offset = 0;
    int backet_position = 0;
    int buf_pointer = 0;
    int elements_count = backet_size;
    for (int backet = 0; backet < backets_count - 1; ++backet) {
        while (buf[buf_pointer] <= splitters[backet]
               && buf_pointer < elements_count
        ) {
            buf[backet*backet_size + backet_position] = buf[buf_pointer];
            ++backet_position;
            ++buf_pointer;
        }
        count_arr[backet] = backet_position;
        backet_position = 0;
    }
    backet_offset = (backets_count-1)*backet_size;
    while (buf_pointer < elements_count) {
        buf[backet_offset + backet_position] = buf[buf_pointer];
        ++backet_position;
        ++buf_pointer;
    }
    count_arr[backets_count - 1] = backet_position;
}

static void test_separate_elements() {

    int arr[64] = {
        3, 10, 18, 30, 31, 33, 40, 49, 51, 64, 66, 69, 70, 77, 79, 91
    };
    int splitters[3] = {30, 51, 72};
    int count_arr[4] = {0};

    separate_elements(
        arr, 64,
        16,
        4,
        count_arr,
        splitters
    );

    int arr_valid1[] = {3, 10, 18, 30};
    int arr_valid2[] = {31, 33, 40, 49, 51};
    int arr_valid3[] = {64, 66, 69, 70};
    int arr_valid4[] = {77, 79, 91};
    for (int q = 0; q < 16; ++q) {
        if (q < 4) check(arr[ 0 + q] == arr_valid1[q]);
        if (q < 5) check(arr[16 + q] == arr_valid2[q]);
        if (q < 4) check(arr[32 + q] == arr_valid3[q]);
        if (q < 3) check(arr[48 + q] == arr_valid4[q]);
    }
    check(count_arr[0] == 4);
    check(count_arr[1] == 5);
    check(count_arr[2] == 4);
    check(count_arr[3] == 3);
}

static void swap_backets(
    int **buckets,
    int *new_backets,
    int backets_count,
    int backets_size,
    int *count_arr
) {

    int *temp_count_arr = (int*) malloc(backets_count * sizeof(int));
    RET_IF_ERR(
        MPI_Alltoall(
            count_arr, 1, MPI_INT,
            temp_count_arr, 1, MPI_INT,
            MPI_COMM_WORLD
        )
    );
    memcpy(count_arr, temp_count_arr, backets_count * sizeof(int));
    free(temp_count_arr);

    RET_IF_ERR(
        MPI_Alltoall(
            *buckets, backets_size, MPI_INT,
            new_backets, backets_size, MPI_INT,
            MPI_COMM_WORLD
        )
    );
}

static inline void merge(
    int *dst,
    const int *src1,
    int count1,
    const int *src2,
    int count2
) {
    int pointer1 = 0;
    int pointer2 = 0;
    int pointer  = 0;
    while (pointer1 < count1 && pointer2 < count2) {
        if (src1[pointer1] < src2[pointer2]) {
            dst[pointer++] = src1[pointer1++];
        } else {
            dst[pointer++] = src2[pointer2++];
        }
    }
    if (pointer1 == count1) {
        memcpy(
            dst + pointer,
            src2 + pointer2,
            (count2 - pointer2) * sizeof(int)
        );
    } else {
        memcpy(
            dst + pointer,
            src1 + pointer1,
            (count1 - pointer1) * sizeof(int)
        );
    }
}

static inline int find_backet_with_min_elem(
    int backet_size,
    int backets_count,
    const int *const *pointers,
    const int *count_arr
) {
    int min = 0;
    int backet_ind = 0;
    for (int q = 0; q < backets_count; ++q) {
        if (count_arr[q] > 0) {
            min = *pointers[q];
            backet_ind = q;
            break;
        }
    }
    for (int q = 0; q < backets_count; ++q) {
        if (count_arr[q] > 0) {
            int elem = *pointers[q];
            if (elem < min) {
                min = elem;
                backet_ind = q;
            }
        }
    }
    return backet_ind;
}

static void merge_backets(
    const int *buf,
    int *help_buf,
    int buf_size,
    int backets_count,
    int *count_arr,
    int *new_count
) {
    int backet_size = buf_size/backets_count;
    int new_buf_size = 0;
    for (int q = 0; q < backets_count; ++q) {
        new_buf_size += count_arr[q];
    }

    const int **pointers = (const int**) malloc(buf_size * sizeof(int*));
    for (int q = 0; q < backets_count; ++q) {
        pointers[q] = buf + q*backet_size;
    }
    for (int buf_pointer = 0; buf_pointer < new_buf_size; ++buf_pointer) {
        int min = find_backet_with_min_elem(
            backet_size, backets_count, pointers, count_arr
        );        
        help_buf[buf_pointer] = *pointers[min];
        count_arr[min] -= 1;
        pointers[min] += 1;
    }

    free(pointers);

    *new_count = new_buf_size;
}

static inline int is_receiver(int rank, int size, int q) {
    return rank % (2*q) == 0 && rank + q < size;
}

static inline int is_sender(int rank, int size, int q) {
    return rank % (2*q) != 0 && 0 < rank;
}

static inline int calc_sender_rank(int rank, int size, int q) {
    return rank + q;
}

static inline int calc_receiver_rank(int rank, int size, int q) {
    return rank - q;
}

static inline void swap_pointers(int **a, int **b) {
    int *t = *b;
    *b = *a;
    *a = t;
}

static inline void update_counts(int *counts_arr, int count, int q) {
    for (int w = 0; w < count; w += 2*q) {
        if (w + q < count) {
            counts_arr[w]     += counts_arr[w + q];
            counts_arr[w + q] =  0;
        }
    }
}

static void gather_backets(
    int *buf_arr, // len = arr size
    int *buf_recv, // len = arr size
    int *buf_merge, // len = arr size
    int *counts_arr, // len = proc count // For elems_counts
    int elements_count,
    int buf_size,
    int rank,
    int size,
    int *ret_arr
) {
    
    RET_IF_ERR(
        MPI_Allgather(
            &elements_count, 1, MPI_INT, 
            counts_arr, 1, MPI_INT, 
            MPI_COMM_WORLD
        )
    );

    const int TAG = 0;
    for (int q = 1; q < size; q *= 2) {
        if (is_receiver(rank, size, q)) {
            int sender_rank = calc_sender_rank(rank, size, q);
            int recv_count = counts_arr[sender_rank];
            RET_IF_ERR(
                MPI_Recv(
                    buf_recv, recv_count, MPI_INT,
                    sender_rank, TAG,
                    MPI_COMM_WORLD, MPI_STATUS_IGNORE
                )
            );
            merge(
                buf_merge,
                buf_arr, counts_arr[rank],
                buf_recv, counts_arr[sender_rank]
            );
            swap_pointers(&buf_merge, &buf_arr);
            update_counts(counts_arr, size, q);
        } else if (is_sender(rank, size, q)) {
            int receiver_rank = calc_receiver_rank(rank, size, q);
            RET_IF_ERR(
                MPI_Send(
                    buf_arr, counts_arr[rank], MPI_INT, 
                    receiver_rank, TAG, MPI_COMM_WORLD
                )
            );
            return;
        }
    }
    if (rank == main_rank) {
        memcpy(ret_arr, buf_arr, buf_size*sizeof(int));
        return;
    }
}

static void separate_on_backets(
    int *self_arr,
    int *count_arr,
    int self_count,
    int count,
    int rank,
    int size
) {
    int splitters_count = size;
    int *splitters = (int*) malloc(splitters_count * sizeof(int));
    calc_splitters(self_arr, splitters, self_count, rank, size);

    separate_elements(
        self_arr, count,
        self_count,
        size,
        count_arr,
        splitters
    );

    free(splitters);
}

static void merge_into_arr(
    int *self_arr,
    int *count_arr,
    int self_count,
    int count,
    int rank,
    int size,
    int *ret_arr
) {
    int new_count = 0;
    int *new_buf1 = (int*) malloc(2 * count * sizeof(int));
    int *new_buf2 = new_buf1 + count;

    swap_backets(&self_arr, new_buf1, size, self_count, count_arr);

    merge_backets(new_buf1, self_arr, count, size, count_arr, &new_count);

    gather_backets(
        self_arr, new_buf1, new_buf2, count_arr,
        new_count, count,
        rank, size,
        ret_arr
    );

    free(new_buf1);
}

void samplesort_alg(int *arr, int count, int rank, int size) {
    int self_count = count / size;
    int *self_arr = (int*) malloc(count * sizeof(int));

    RET_IF_ERR(
        MPI_Scatter(
            arr, self_count, MPI_INT, 
            self_arr, self_count, MPI_INT, 
            main_rank, MPI_COMM_WORLD
        )
    );

    quicksort(self_arr, 0, self_count-1);

    int *count_arr = (int*) malloc(size * sizeof(int));
    separate_on_backets(
        self_arr, count_arr, self_count, count,
        rank, size
    );

    merge_into_arr(
        self_arr, count_arr, self_count, count,
        rank, size, 
        arr
    );

    free(self_arr);
    free(count_arr);
}

void samplesort(int *arr, int count, int rank, int size) {
    check(rank != 0 || arr);
    check(size > 1);
    check(count % size == 0);

    samplesort_alg(arr, count, rank, size);
}

void get_arr_for_testing_samplesort(int **ret_arr, int *ret_count) {
    /**
     * The sorting of this array must be performed by three processors.
     * After local sort
     *    #0  1  2  3  4  5  6  7  8
     *     2  6  7  8 10 12 14 20 26
     *     1  4 13 15 17 18 19 23 25
     *     3  5  9 11 16 21 22 24 27
     * Indexes of the auxiliary array
     *     0, 3, 6
     * Elements of the auxiliary array
     *     2  8 14
     *     1 15 19
     *     3 11 22
     * Merged auxiliary array
     *     1  2  3  8 11 14 15 19 22
     * Selected splitters(by indexes: 4, 7):
     *     11 19
     * Separation into buckets
     *     2  6  7  8 10 | 12 14 | 20 26
     *     1  4 | 13 15 17 18 19 | 23 25
     *     3  5  9 11 | 16 | 21 22 24 27
     * Joined buckets on every processor
     *     2 6 7 8 10 1 4 3 5 9 11 
     *     12 14 13 15 17 18 19 16
     *     20 26 23 25 21 22 24 27
     * Sorted array
     *     1 2 3 4 5 6 7 8 9 10 11 12 13 14 
     *     15 16 17 18 19 20 21 22 23 24 25 26 27
     */
    static int arr[] = {
        20,  8, 14,  7, 26, 12,  6,  2, 10,
        19,  1, 23, 18, 25, 17, 13, 15,  4,
        9 ,  3, 21, 24,  5, 22, 16, 11, 27,
    };
    const int count = sizeof(arr)/sizeof(arr[0]);

    *ret_arr = arr;
    *ret_count = count;
}

// ------------------------------------------------------------------------

typedef enum Mode_t {
    MODE_EXEC_HEAP = 1,
    MODE_EXEC_QUICK,
    MODE_EXEC_RADIX,
    MODE_EXEC_SAMPLE,
    MODE_CHECK_HEAP,
    MODE_CHECK_QUICK,
    MODE_CHECK_RADIX,
    MODE_CHECK_SAMPLE,
} Mode;

int is_mode_correct(Mode mode) {
    return 0 < mode && mode < 9;
}

int is_mode_exec(Mode mode) {
    return 0 < mode && mode < 5;
}

int is_mode_check(Mode mode) {
    return 4 < mode && mode < 9;
}

void test_exec_time(Mode mode, int count_per_proc, int rank, int size) {
    int max = count_per_proc;
    int points_count = 100;
    for (int q = 1; q < max; q += max/points_count) {
        int count =  q * (mode == MODE_EXEC_SAMPLE ? size : 1);
        int *arr = NULL;
        if (rank == main_rank) {
            arr = generate_random_array(count, INT_MAX, 7);
        }
        RET_IF_ERR(MPI_Barrier(MPI_COMM_WORLD));
        
        clock_t start, end;
        if (rank == 0) {
            start = clock();
        }

        switch (mode) {
            case MODE_EXEC_HEAP:
                if (rank == main_rank) { heapsort(arr, count); }
                break;
            case MODE_EXEC_QUICK:  
                if (rank == main_rank) { quicksort(arr, 0, count - 1); }
                break;
            case MODE_EXEC_RADIX:  
                if (rank == main_rank) { radixsort(arr, count); }
                break;
            case MODE_EXEC_SAMPLE:
                samplesort(arr, count, rank, size);
                break;
            default: check_ames(0, "Incorect mode");
        }

        if (rank == 0) {
            end = clock();
            double delta_time = (double)(end - start) / CLOCKS_PER_SEC;
            // printf("time: %f on %d elements\n", delta_time, count);
            printf("(%f, %d),\n", delta_time, count);
            // printf("correct: %s\n", check_arr(arr, count) ? "true" : "false");
            free(arr);
        }
    }
}

void wrapper_heapsort(int *arr, int count, int rank, int size) {
    heapsort(arr, count);
}

void wrapper_quicksort(int *arr, int count, int rank, int size) {
    quicksort(arr, 0, count - 1);
}

void wrapper_samplesort(int *arr, int count, int rank, int size) {
    samplesort(arr, count, rank, size);
}

void test_correctness(Mode mode, int count_per_proc, int rank, int size) {
    clock_t start, end;
    int count = count_per_proc*size;
    int *arr = NULL;
    if (rank == main_rank) {
        printf("Sorting random array of len %d\n", count);
        arr = generate_random_array(count, 1000, 7);
        // arr = (int*) malloc(count * sizeof(int));
        // for (int q = 0; q < count; ++q) { arr[q] = 7; }
        start = clock();
    }

    switch (mode) {
        case MODE_CHECK_HEAP:
            if (rank == main_rank) { heapsort(arr, count); }
            break;
        case MODE_CHECK_QUICK:  
            if (rank == main_rank) { quicksort(arr, 0, count - 1); }
            break;
        case MODE_CHECK_RADIX:  
            if (rank == main_rank) { radixsort(arr, count); }
            break;
        case MODE_CHECK_SAMPLE:
            samplesort(arr, count, rank, size);
            break;
        default: check_ames(0, "Incorect mode");
    }

    if (rank == main_rank) {
        end = clock();
        double delta_time = (double)(end - start) / CLOCKS_PER_SEC;
        printf("time: %f\n", delta_time);
        printf("correct: %s\n", check_arr(arr, count) ? "true" : "false");
        free(arr);
    }
}

void parse_params(
    int argc,
    char **argv,
    Mode *ret_mode,
    int *ret_count_per_proc
) {
    int rank, size;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));

    check_ames(
        size > 1,
        "You should specify more than 1 proc for this program!"
    );
    check_ames(
        argc == 3,
        "You should specify 2 arguments: mode and count_per_proc"
    );

    Mode mode = atoi(argv[1]);
    int count_per_proc = atoi(argv[2]);
    check_ames(is_mode_correct(mode), "Mode must be int in range [1, 6]");
    check_ames(0 < count_per_proc, "count_per_proc must be positive int");

    *ret_mode = mode;
    *ret_count_per_proc = count_per_proc;
}

// ------------------------------------------------------------------- main

int main(int argc, char **argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    int rank, size;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));
    
    Mode mode = 0;
    int count_per_proc = 0;
    parse_params(argc, argv, &mode, &count_per_proc);

    if (is_mode_exec(mode)) {
        test_exec_time(mode, count_per_proc, rank, size);
    } else if (is_mode_check(mode)) {
        test_correctness(mode, count_per_proc, rank, size);
    } else {
        check_ames(0, "Unknown mode");
    }

    MPI_Finalize();

    return 0;
}
