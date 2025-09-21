#include <bits/types/sigevent_t.h>
#include <pthread.h>
#include <stdio.h>



const int size = 7;
const int count = 177;

typedef struct Data_t {
    int rank;
    int from;
    int to;
    double *sum;
} Data;

void *func(void *data) {
    Data *dat = (Data*)data;
    int rank = dat->rank;
    
    for (int q = dat->from; q < dat->to; ++q) {
        *dat->sum += 1.0 / (q + 1);
    }

    return NULL;
}

int main() {

    pthread_t pthread_arr[size];
    Data data_arr[size];
    double sum_arr[size];
    for (int rank = 0; rank < size; ++rank) {
        sum_arr[rank] = 0;
        data_arr[rank] = (Data) {
            .rank = rank,
            .from = count/size*rank,
            .to   = ((rank + 1 == size) ? (count)
                                        : (count/size*rank + count/size)),
            .sum  = (&sum_arr[rank])
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
    
    double sum = 0;
    for (int rank = 0; rank < size; ++rank) {
        if (rank != 0) {
            pthread_join(pthread_arr[rank], NULL);
        }
        sum += sum_arr[rank];
    }
    printf("sum from 1 to 1/%d is %f.\n", count, sum);

    return 0;
}
