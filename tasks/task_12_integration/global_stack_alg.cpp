#include <assert.h>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <pthread.h>
#include <thread>

#include "range.h"
#include "stack.h"

#include "../../slibs/err_proc.h"


// #undef DOT
#define DRT printf("\033[95mDOT rank %d from line: %d, %s\033[39m\n", data.rank, __LINE__, __FILE__);

constexpr std::size_t local_stack_size  = 1000;
constexpr std::size_t global_stack_size = 1000;
constexpr int main_rank = 0;

static bool stop_signal = false;

static pthread_mutex_t global_stack_mutex;
static pthread_cond_t global_stack_cond;
static int global_stack_waiters = 0;
void global_stack_mutex_init() {
    pthread_mutex_init(&global_stack_mutex, nullptr);
    pthread_cond_init(&global_stack_cond, nullptr);
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
void global_stack_increase_waiters() {
    ++global_stack_waiters;
}
void global_stack_decrease_waiters() {
    --global_stack_waiters;
}


struct ThreadData {
    Stack* global_stack;
    Stack* local_stack;
    Stack* local_stack_arr;
    int mean;
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
        mean /= data.size;
        data.mean = mean;
    }
    if (stack.get_occupancy() > data.mean) {
        global_stack_mutex_lock();
        for (int q = 0; q < stack.get_occupancy() - data.mean; ++q) {
            Range range = stack.pop();
            global_stack.push(range);
        }
        global_stack_broadcast_event();
        global_stack_mutex_unlock();
    } else if (stack.get_occupancy() < data.mean) {
        global_stack_mutex_lock();
        int count = std::min(
            global_stack.get_occupancy(),
            data.mean - stack.get_occupancy()
        );
        for (int q = 0; q < count; ++q) {
            Range range = global_stack.pop();
            stack.push(range);
        }
        global_stack_mutex_unlock();
    }
}

bool take_elements_from_global_stack(Stack& stack, Stack& global_stack, int size) {
    global_stack_mutex_lock();
    if (!global_stack.is_empty()) {
        int count = global_stack.get_occupancy()/size + global_stack.get_occupancy()%size;
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

    // global_stack_mutex_lock();
    // std::cout << "proc: " << data.rank << std::endl;
    // stack.print_stack();
    // global_stack_mutex_unlock();
    // return nullptr;

    int balance_time = 0;
    while (true) {
        if (!stack.is_empty()) {
            Range cur_range = stack.pop();
            double sabc = cur_range.calc_area();
            double c = cur_range.calc_mid_point();
            double fc = data.f(c);
            if (!cur_range.calc_cond(data.eps, c, fc)) {
                stack.push(cur_range.split_range(c, fc));
                stack.push(cur_range);
            } else {
                *data.sum += sabc;
            }
            if (balance_time == 10) {
                
                balance_elements(data, stack, global_stack);
                balance_time = 0;
            } else {
                ++balance_time;
            }
        } else {
            if (!take_elements_from_global_stack(stack, global_stack, data.size)) {
                break;
            }
            // break;
        }
    }

    return nullptr;
}

double global_stack_alg(
    Range range,
    double (*f)(double),
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

	// pthread_t* thread_arr = static_cast<pthread_t*>(
    //     ::operator new[](proc_count * sizeof(pthread_t))
    // );
    pthread_t* thread_arr = new pthread_t[proc_count];
    ThreadData* thread_data_arr = new ThreadData[proc_count];
    double* sum_arr = new double[proc_count];

    for (int q = 0; q < proc_count; ++q) {
        sum_arr[q] = 0;
        thread_data_arr[q] = {
            .global_stack = &global_stack,
            .local_stack  = &local_stack_arr[q],
            .local_stack_arr = local_stack_arr,
            .mean = count_per_proc,
            .rank = q,
            .size = proc_count,
            .f = f,
            .eps = 0.0001,
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

    return 0;
}
