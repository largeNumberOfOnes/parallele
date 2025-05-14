#include <pthread.h>
#include <stdio.h>



const int size = 7;
const int count = 177;

typedef struct Data_t {
    int rank;
    pthread_mutex_t *mutex;
    int *var;
} Data;

void *func(void *data) {
    Data *dat = (Data*)data;
    int rank = dat->rank;
    
    pthread_mutex_lock(dat->mutex);

    printf("%d -> var: %d to %d\n", rank, *dat->var, *dat->var + 7);
    *dat->var += 7;

    pthread_mutex_unlock(dat->mutex);

    return NULL;
}

int main() {

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_t pthread_arr[size];
    Data data_arr[size];
    int var = 0;
    for (int rank = 0; rank < size; ++rank) {
        data_arr[rank] = (Data) {
            .rank = rank,
            .mutex = &mutex,
            .var  = &var
        };
        if (rank != 0) {
            pthread_create(
                &pthread_arr[rank],
                NULL,
                func,
                (void*)(&data_arr[rank])
            );
        }
    }
    func((void*)(&data_arr[0]));
    
    for (int rank = 0; rank < size; ++rank) {
        if (rank != 0) {
            pthread_join(pthread_arr[rank], NULL);
        }
    }

    pthread_mutex_destroy(&mutex);

    return 0;
}
