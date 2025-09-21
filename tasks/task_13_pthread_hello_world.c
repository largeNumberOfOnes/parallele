#include <pthread.h>
#include <stdio.h>



const int size = 7;

void *func(void *data) {
    int rank = *(int*)data;
    printf("Hello to procs");
    for (int q = 0; q < size; ++q) {
        if (q != rank) {
            printf(" %d,", q);
        }
    }
    printf("\b!!! I am proc with rank %d!\n", rank);

    return NULL;
}

int main() {

    pthread_t pthread_arr[size];
    int rank_num_arr[size];
    for (int q = 0; q < size; ++q) {
        rank_num_arr[q] = q;
        if (q != 0) {
            pthread_create(
                &pthread_arr[q],
                NULL,
                func,
                (void*)(&rank_num_arr[q])
            );
        }
    }
    func((void*)(&rank_num_arr[0]));

    return 0;
}
