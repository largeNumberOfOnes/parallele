#include <assert.h>
#include <cmath>
#include <iostream>
#include <pthread.h>

#include "range.h"
#include "stack.h"



constexpr std::size_t local_stack_size  = 10000;
constexpr std::size_t global_stack_size = 10000;
constexpr int main_rank = 0;

static bool stop_signal = false;

static pthread_mutex_t global_stack_mutex;
static pthread_cond_t global_stack_cond;
static int global_stack_waiters = 0;
void global_stack_mutex_init() {
    if (false
        || !pthread_mutex_init(&global_stack_mutex, nullptr)
        || !pthread_cond_init(&global_stack_cond, nullptr)
    ) {
        assert(0);
    }
}
void global_stack_mutex_destroy() {
    if (false
        || !pthread_mutex_destroy(&global_stack_mutex)
        || !pthread_cond_destroy(&global_stack_cond)
    ) {
        assert(0);
    }
}
void global_stack_mutex_lock() {
    pthread_mutex_lock(&global_stack_mutex);
}
void global_stack_mutex_unlock() {
    pthread_mutex_unlock(&global_stack_mutex);
}
void global_stack_wait_event() {
    ++global_stack_waiters;
    pthread_cond_wait(&global_stack_cond, &global_stack_mutex);
    --global_stack_waiters;
}
void global_stack_broadcast_event() {
    pthread_cond_broadcast(&global_stack_cond);
}


struct ThreadData {
    Stack* global_stack;
    Stack* local_stack;
    Stack* local_stack_arr;
    int* mean;
    int rank;
    int size;
    double (*f)(double);
    double eps;
    double* sum;
};

void balance_elements(ThreadData& data, Stack& stack, Stack& global_stack) {
    if (data.rank == main_rank) {
        int mean = 0;
        for (int q = 0; q < data.size; ++q) {
            mean += data.local_stack_arr[q].get_occupancy();
        }
        mean += global_stack.get_occupancy();
        mean /= data.size;
        *data.mean = mean;
    }
    global_stack_mutex_lock();
    int mean = *data.mean;
    if (stack.get_occupancy() > mean) {
        Stack::move(stack, global_stack, stack.get_occupancy() - mean);
        global_stack_broadcast_event();
    } else if (stack.get_occupancy() < mean) {
        int count = std::min(
            global_stack.get_occupancy(),
            mean - stack.get_occupancy()
        );
        Stack::move(global_stack, stack, count);
    }
    global_stack_mutex_unlock();
}

bool replenish_elements(Stack& stack, Stack& global_stack, int size) {
    global_stack_mutex_lock();
    if (!global_stack.is_empty()) {
        int count = global_stack.get_occupancy() / size
                  + global_stack.get_occupancy() % size;
        for (int q = 0; q < count; ++q) {
            Range range = global_stack.pop();
            stack.push(range);
        }
    } else {
        while (global_stack.is_empty()) {
            if (global_stack_waiters == size-1) {
                stop_signal = true;
                global_stack_broadcast_event();
                global_stack_mutex_unlock();
                return false;
            }
            global_stack_wait_event();
            if (stop_signal) {
                global_stack_mutex_unlock();
                return false;
            }
        }
    }
    global_stack_mutex_unlock();

    return true;
}

void* thread_function(void* void_data) {

    ThreadData& data = *reinterpret_cast<ThreadData*>(void_data);
    Stack& stack = *data.local_stack;
    Stack& global_stack = *data.global_stack;

    int balance_time = 0;
    while (true) {
        if (!stack.is_empty()) {
            Range cur_range = stack.pop();
            if (!cur_range.is_valid()) {
                continue;
            }
            double sabc = cur_range.calc_area();
            double c = cur_range.calc_mid_point();
            double fc = data.f(c);
            if (!cur_range.calc_cond(data.eps, c, fc)) {
                Range range1 = cur_range.split_range(c, fc);
                if (range1.is_valid()) {
                    stack.push(range1);
                }
                if (cur_range.is_valid()) {
                    stack.push(cur_range);
                }
            } else {
                *data.sum += sabc;
            }
            if (balance_time == 40) {
                
                balance_elements(data, stack, global_stack);
                balance_time = 0;
            } else {
                ++balance_time;
            }
        } else {
            if (!replenish_elements(stack, global_stack, data.size)) {
                break;
            }
        }
    }

    return nullptr;
}

double global_stack_alg(
    Range range,
    double (*f)(double),
    double eps,
    int proc_count
) {
    assert(range.is_valid());
    assert(f);
    assert(proc_count > 0);

    global_stack_mutex_init();

    Stack global_stack{global_stack_size};
    Stack* local_stack_arr = reinterpret_cast<Stack*>(
        ::operator new[](proc_count * sizeof(Stack))
    );
    constexpr int count_per_proc = 10;
    double delta = range.get_len() / count_per_proc / proc_count;
    for (int q = 0; q < proc_count; ++q) {
        new(&local_stack_arr[q]) Stack{local_stack_size};
        for (int w = 0; w < count_per_proc; ++w) {
            local_stack_arr[q].push(Range{
                range.get_a() + (q*count_per_proc + w) * delta,
                range.get_a() + (q*count_per_proc + w + 1) * delta,
                f
            });
        }
    }

    pthread_t* thread_arr = new pthread_t[proc_count];
    ThreadData* thread_data_arr = new ThreadData[proc_count];
    double* sum_arr = new double[proc_count];
    int mean_count = count_per_proc;

    for (int q = 0; q < proc_count; ++q) {
        sum_arr[q] = 0;
        thread_data_arr[q] = {
            .global_stack = &global_stack,
            .local_stack  = &local_stack_arr[q],
            .local_stack_arr = local_stack_arr,
            .mean = &mean_count,
            .rank = q,
            .size = proc_count,
            .f = f,
            .eps = eps,
            .sum = &sum_arr[q]
        };
        if (q != 0) {
            pthread_create(
                &(thread_arr[q]),
                NULL,
                thread_function,
                reinterpret_cast<void*>(&thread_data_arr[q])
            );
        }
    }
    thread_function(reinterpret_cast<void*>(&thread_data_arr[0]));

    for (int q = 1; q < proc_count; ++q) {
        pthread_join(thread_arr[q], nullptr);
    }
    global_stack.print_stack();

    double sum = 0;
    for (int q = 0; q < proc_count; ++q) {
        std::cout << sum_arr[q] << " ";
        sum += sum_arr[q];
    }
    std::cout << "\n sum: " << sum << std::endl;

    for (int q = 0; q < proc_count; ++q) {
        local_stack_arr[q].~Stack();
    }

    delete [] sum_arr;
    delete [] thread_data_arr;
    delete [] thread_arr;
    ::operator delete[](local_stack_arr);

    global_stack_mutex_destroy();

    return 0;
}
