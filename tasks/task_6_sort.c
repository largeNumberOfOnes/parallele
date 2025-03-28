

#include "mpi.h"
#include "../slibs/err_proc.h"
#include "mpi_proto.h"
#include <assert.h>
#include <bits/types/struct_tm.h>
#include <stdio.h>
#include <stdlib.h>

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
    TEST_MACRO(0, 0, 0);
    TEST_MACRO(1, 1, 1);
    TEST_MACRO(7, 3, 4);
    TEST_MACRO(8, 4, 1);
    TEST_MACRO(10, 4, 3);
    TEST_MACRO(15, 4, 8);
    TEST_MACRO(16, 5, 1);
    TEST_MACRO(20, 5, 5);
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

#pragma region 

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

#pragma endregion

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

    // 4. Распределение элементов по процессам
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

    // 5. Обмен размерами данных между процессами
    MPI_Alltoall(send_counts, 1, MPI_INT, recv_counts, 1, MPI_INT, comm);

    // 6. Подготовка к пересылке данных
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

    // 7. Обмен данными (MPI_Alltoallv)
    MPI_Alltoallv(*arr, send_counts, send_displs, MPI_INT,
                  recv_buf, recv_counts, recv_displs, MPI_INT, comm);

    // 8. Освобождение старого массива и замена на новый
    free(*arr);
    *arr = recv_buf;
    *n = total_recv;

    // 9. Финальная локальная сортировка
    // insertion_sort(*arr, *n);

    free(splitters);
    free(send_counts);
    free(recv_counts);
    free(send_displs);
    free(recv_displs);
}

const int main_rank = 0;

int sample_sort(int *arr, int count, int rank, int size) {

    int self_count = count / size;
    int *self_arr = (int*) malloc(self_count*sizeof(int));
    int self_splitters_count = size - 1;
    int *self_splitters = (int*) malloc(self_splitters_count*sizeof(int));

    int splitters_count = self_splitters_count * size;
    int *splitters = (rank == main_rank)
                        ? ((int*)malloc(splitters_count*sizeof(int)))
                        : NULL; 

    RET_IF_ERR(
        MPI_Scatter(
            arr, self_count, MPI_INT,
            self_arr, self_count, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );
    print_arr(self_arr, self_count);

    quicksort(self_arr, 0, self_count - 1);

    for (int q = 0; q < self_splitters_count; ++q) {
        self_splitters[q] = self_arr[q*self_count/size];
    }

    RET_IF_ERR(
        MPI_Gather(
            self_splitters, self_splitters_count, MPI_INT,
            splitters, splitters_count, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );

    if (rank == main_rank) {
        quicksort(splitters, 0, splitters_count);
        for (int q = 0; q < self_splitters_count - 1; ++q) {
            self_splitters[q] = splitters[(q + 1)*splitters_count/size];
        }
    }
    RET_IF_ERR(
        MPI_Bcast(
            self_splitters, self_splitters_count, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );

    int *send_counts = (int *)calloc(size, sizeof(int));
    int *recv_counts = (int *)calloc(size, sizeof(int));
    for (int i = 0; i < self_count; i++) {
        int x = self_arr[i];
        int dest = 0;
        while (dest < size - 1 && x >= self_splitters[dest]) {
            RET_IF_ERR(dest >= size-1);
            RET_IF_ERR(dest < 0)
            dest++;
        }
        send_counts[dest]++;
    }
    RET_IF_ERR(
        MPI_Alltoall(
            send_counts, 1, MPI_INT,
            recv_counts, 1, MPI_INT,
            MPI_COMM_WORLD
        )
    );
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

    // 7. Обмен данными (MPI_Alltoallv)
    RET_IF_ERR(
        MPI_Alltoallv(
            self_arr, send_counts, send_displs, MPI_INT,
            recv_buf, recv_counts, recv_displs, MPI_INT,
            MPI_COMM_WORLD
        )
    );

    quicksort(self_arr, 0, self_count - 1);

    RET_IF_ERR(
        MPI_Gather(
            self_arr, self_count, MPI_INT, 
            arr, count, MPI_INT,
            main_rank, MPI_COMM_WORLD
        )
    );

    return 0;
}

int main(int argc, char **argv) {

    RET_IF_ERR(MPI_Init(&argc, &argv));

    int rank, size;
    RET_IF_ERR(MPI_Comm_rank(MPI_COMM_WORLD, &rank));
    RET_IF_ERR(MPI_Comm_size(MPI_COMM_WORLD, &size));

    // test_calc_layers();
    // int arr[] = {4, 2, 21, 4, 2, 9, -3, 2, 6, 7};
    // int count = sizeof(arr)/sizeof(arr[0]);
    // heap_sort(arr, count);

    int count = size*15;
    srand(7);
    int *arr = generate_random_array(count, 1000);

    if (rank == main_rank) {
        print_arr(arr, count);
    }
    sample_sort(arr, count, rank, size);
    if (rank == main_rank) {
        print_arr(arr, count);
    }
    if (rank != main_rank) {
        return 0;
    }
    
    int correct = 1;
    for (int q = 1; q < count; ++q) {
        if (arr[q] < arr[q-1]) {
            correct = 0;
            break;
        }
    }
    printf("correct: %d\n", correct);

    MPI_Finalize();

    return 0;
}
