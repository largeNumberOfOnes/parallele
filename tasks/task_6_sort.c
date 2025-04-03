

#include "mpi.h"
#include "../slibs/err_proc.h"
#include <assert.h>
#include <bits/types/struct_tm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_arr(int *arr, int count) {
    for (int q = 0; q < count; ++q) {
        printf("%d, ", arr[q]);
    }
    printf("\n");
}

void calc_layers(int count, int *layers, int *reminder) {
    int ret_layers = 0;
    for (int q = 1; count > q; q <<= 1) {
        count -= q;
        ++ret_layers;
    }
    if (count > 0) {
        ++ret_layers;
    }

    *layers = ret_layers;
    *reminder = count;
}

void test_calc_layers() {
    int layers, reminder;
    #define TEST_MACRO(count, lay_exp, rem_exp) {                         \
        calc_layers((count), &layers, &reminder);                         \
        assert(layers == (lay_exp));                                      \
        assert(reminder == (rem_exp));                                    \
    }
    TEST_MACRO(  0, 0, 0);
    TEST_MACRO(  1, 1, 1);
    TEST_MACRO(  7, 3, 4);
    TEST_MACRO(  8, 4, 1);
    TEST_MACRO( 10, 4, 3);
    TEST_MACRO( 15, 4, 8);
    TEST_MACRO( 16, 5, 1);
    TEST_MACRO( 20, 5, 5);
    TEST_MACRO(128, 8, 1);
    TEST_MACRO(340, 9, 85);

    #undef TEST_MACRO
}

void swap(int *a, int *b) {
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

void heapsort_(int *arr, int count) {
    assert(arr);
    assert(count > 0);

    int layers = 0, reminder = 0;
    calc_layers(count, &layers, &reminder);
    int last_root = 0;
    int first_root = 1 + count - reminder;
    for (int layer = layers - 2; layer >= 0; --layer) {
        last_root = first_root - 1;
        first_root = last_root - (1 << layer) + 1;
        // printf("first_root = %d\nlast_root = %d\n", first_root, last_root);
        for (int root = first_root; root < last_root + 1; ++root) {
            int l = 2*root + 1;
            int r = 2*root + 2;
            if (r < count && arr[r] > arr[root]) {
                swap(&arr[r], &arr[root]);
            }
            if (l < count && arr[l] > arr[root]) {
                swap(&arr[l], &arr[root]);
            }
            if (r < count && l < count && arr[r] > arr[l]) {
                swap(&arr[r], &arr[l]);
            }
        }
    }
}

void heapsort(int *arr, int count) {
    assert(arr);
    assert(count > 0);

    for (int q = count; q > 0; --q) {
        heapsort_(arr, q);
        swap(&arr[0], &arr[q-1]);
    }
}

void heapify(int *arr, int n, int i) {
    assert(arr);
    assert(n > 0);

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

void heap_sort(int *arr, int n) {
    for (int i = n / 2 - 1; i > -1; --i)
        heapify(arr, n, i);

    for (int i = n - 1; i > 0; --i) {
        int tmp = arr[0];
        arr[0] = arr[i];
        arr[i] = tmp;
        heapify(arr, i, 0);
    }
}

int partition(int *arr, int l, int r) {
    int v = arr[(l + r) / 2];
    int i = l;
    int j = r;
    while (i <= j) {
        while (arr[i] < v)
           i++;
        while (arr[j] > v)
           j--;
        if (i >= j) 
           break;
        swap(&arr[i++], &arr[j--]);
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

int *generate_random_array(int count, int max) {
    int *arr = (int*) malloc(count*sizeof(int));
    for (int q = 0; q < count; ++q) {
        arr[q] = rand() % max;
    }
    return arr;
}

// void insertion_sort(int *arr, )

void no_sample_sort(int **arr, int *n, MPI_Comm comm) {
    int rank, size;
    MPI_Comm_rank(comm, &rank);
    MPI_Comm_size(comm, &size);

    // 1. Локальная сортировка
    // insertion_sort(*arr, *n);

    // 2. Выбор сплиттеров (p-1 элементов)
    int *splitters = (int *)malloc((size - 1) * sizeof(int));
    for (int i = 0; i < size - 1; i++) {
        splitters[i] = (*arr)[(*n * (i + 1)) / size];
    }

    // 3. Сбор всех сплиттеров в процессе 0 и рассылка
    int *global_splitters = NULL;
    if (rank == 0) {
        global_splitters = (int *)malloc(size * (size - 1) * sizeof(int));
    }
    MPI_Gather(splitters, size - 1, MPI_INT, 
               global_splitters, size - 1, MPI_INT, 0, comm);

    if (rank == 0) {
        // insertion_sort(global_splitters, size * (size - 1));
        for (int i = 0; i < size - 1; i++) {
            splitters[i] = global_splitters[(i + 1) * size];
        }
        free(global_splitters);
    }
    MPI_Bcast(splitters, size - 1, MPI_INT, 0, comm);

    int *send_counts = (int *)calloc(size, sizeof(int));
    int *recv_counts = (int *)calloc(size, sizeof(int));
    for (int i = 0; i < *n; i++) {
        int x = (*arr)[i];
        int dest = 0;
        while (dest < size - 1 && x >= splitters[dest]) {
            dest++;
        }
        send_counts[dest]++;
    }

    MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, comm);

    int total_recv = 0;
    for (int i = 0; i < size; i++) {
        total_recv += recv_counts[i];
    }

    int *recv_buf = (int *)malloc(total_recv * sizeof(int));
    int *send_displs = (int *)calloc(size, sizeof(int));
    int *recv_displs = (int *)calloc(size, sizeof(int));

    for (int i = 1; i < size; i++) {
        send_displs[i] = send_displs[i - 1] + send_counts[i - 1];
        recv_displs[i] = recv_displs[i - 1] + recv_counts[i - 1];
    }

    MPI_Alltoallv(*arr, send_counts, send_displs, MPI_INT,
                  recv_buf, recv_counts, recv_displs, MPI_INT, comm);

    free(*arr);
    *arr = recv_buf;
    *n = total_recv;

    // 9. Финальная локальная сортировка
    // insertion_sort(*arr, *n);
    // quicksort(arr, 0, count - 1);

    free(splitters);
    free(send_counts);
    free(recv_counts);
    free(send_displs);
    free(recv_displs);
}

const int main_rank = 0;

static void calc_splitters(
    int *self_arr,
    int count,
    int rank,
    int size,
    int **ret_splitters,
    int *ret_splitters_count
) {
    int splitters_count = size;
    int *splitters = (int*) malloc(splitters_count * sizeof(int));
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
    }
    if (rank == main_rank) {
        int offset = size/2 - 1;
        for (int q = 1; q < size; ++q) {
            splitters[q-1] = all_splitters[q*size + offset];
        }
    }
    free(all_splitters);

    RET_IF_ERR(
        MPI_Bcast(
            splitters, splitters_count-1, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );

    *ret_splitters = splitters;
    *ret_splitters_count = splitters_count;
}

static void separate_elements(
    int *buf, int buf_size,
    int elements_count,
    int backets_count,
    int *count_arr,
    int *splitters
) {

    int element_position = 0;

    for (int q = 0; q < elements_count; ++q) {
        if (buf[q] > splitters[0]) {
            element_position = q;
            count_arr[0] = q;
            break;
        }
    }

    int backet_offset = elements_count;
    int backet_position = 0;
    int splitters_position = 1;
    for (int q = element_position; q < elements_count; ++q) {
        if (buf[q] > splitters[splitters_position]
            && splitters_position < backets_count-1
        ) {
            count_arr[splitters_position] = backet_position;   
            ++splitters_position;
            backet_offset += elements_count;
            backet_position = 0;
        }
        buf[backet_offset + backet_position] = buf[q];
        ++backet_position;
    }
    count_arr[backets_count - 1] = backet_position;

}

void test_separate_elements() {

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

static void swap_buckets(
    int **buckets,
    int buckets_count,
    int buckets_size,
    int **count_arr
) {

    int *new_count_arr = (int*) malloc(buckets_count * sizeof(int));
    RET_IF_ERR(
        MPI_Alltoall(
            *count_arr, 1, MPI_INT,
            new_count_arr, 1, MPI_INT,
            MPI_COMM_WORLD
        )
    );
    free(*count_arr);
    *count_arr = new_count_arr;

    int *new_backets = (int*) malloc(buckets_count * buckets_size *
                                                            sizeof(int));
    RET_IF_ERR(
        MPI_Alltoall(
            *buckets, buckets_size, MPI_INT,
            new_backets, buckets_size, MPI_INT,
            MPI_COMM_WORLD
        )
    );
    free(*buckets);
    *buckets = new_backets;
}

static void merge(
    int *dst,
    int *src1,
    int count1,
    int *src2,
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
        while (pointer2 < count2) {
            dst[pointer++] = src2[pointer2++];
        }
    } else {
        while (pointer1 < count1) {
            dst[pointer++] = src1[pointer1++];
        }
    }
}

inline int get_elem(
    const int *buf,
    int backet_size,
    int backet,
    int element
) {
    return buf[backet*backet_size + element];
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
    int **buf,
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

    int *new_buf = (int*) malloc(buf_size * sizeof(int));
    const int **pointers = (const int**) malloc(buf_size * sizeof(int*));
    for (int q = 0; q < backets_count; ++q) {
        pointers[q] = *buf + q*backet_size;
    }
    for (int buf_pointer = 0; buf_pointer < new_buf_size; ++buf_pointer) {
        int min = find_backet_with_min_elem(
            backet_size, backets_count, pointers, count_arr
        );        
        new_buf[buf_pointer] = *pointers[min];
        count_arr[min] -= 1;
        pointers[min] += 1;
    }

    free(*buf);
    *buf = new_buf;
    *new_count = new_buf_size;
}

static void gather_backets(
    int *buf,
    int buf_size,
    int rank,
    int size
) {
    
    for (int q = 0; q < ; ++q) {
        if (rank % 2 == 0 && rank + 1 != size) {
            // recv
        } else {
            // send
        }
    }
}

int sample_sort_alg(int *arr, int count, int rank, int size) {
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

    int splitters_count = size;
    int *splitters = NULL;
    calc_splitters(
        self_arr, self_count, rank, size,
        &splitters, &splitters_count
    );

    int *count_arr = (int*) malloc(size * sizeof(int));
    separate_elements(
        self_arr, count,
        self_count,
        size,
        count_arr,
        splitters
    );

    swap_buckets(&self_arr, size, self_count, &count_arr);
    
    int new_count = 0;
    merge_backets(&self_arr, count, size, count_arr, &new_count);

    // rprint_arr(self_arr, new_count);

    gather_backets();


    return 0;
}

int sample_sort(int *arr, int count, int rank, int size) {

    check(rank != 0 || arr);
    check(size > 1);
    check(count % size == 0);

    RET_IF_ERR(
        sample_sort_alg(arr, count, rank, size)
    );

    return 0;
}

int main(int argc, char **argv) {
    RET_IF_ERR(MPI_Init(&argc, &argv));

    // test_separate_elements();
    // return 0;

    int rank, size;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));

    check_ames(
        size > 1,
        "You should specify more than 1 proc for this program!"
    )

    // test_calc_layers();
    int arr[] = {
        20,  8, 14,  7, 26, 12,  6,  2, 10,
        19,  1, 23, 18, 25, 17, 13, 15,  4,
        9 ,  3, 21, 24,  5, 22, 16, 11, 27,
    };
    int count = sizeof(arr)/sizeof(arr[0]);
    // heap_sort(arr, count);

    // int count = size*15;
    // srand(7);
    // int *arr = NULL;
    // if (rank == main_rank) {
    //     generate_random_array(count, 1000);
    // }

    // if (rank != main_rank) {
    //     arr = NULL;
    // }

    // if (rank == main_rank) {
    //     print_arr(arr, count);
    // }
    sample_sort(arr, count, rank, size);
    // if (rank == main_rank) {
    //     print_arr(arr, count);
    // }
    // if (rank != main_rank) {
    //     return 0;
    // }
    
    if (rank == main_rank) {
        int correct = 1;
        for (int q = 1; q < count; ++q) {
            if (arr[q] < arr[q-1]) {
                correct = 0;
                break;
            }
        }
        printf("correct: %s\n", correct ? "true" : "false");
    }

    MPI_Finalize();

    return 0;
}
