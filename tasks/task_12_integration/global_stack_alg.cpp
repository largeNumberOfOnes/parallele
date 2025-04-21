


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


constexpr std::size_t local_stack_size  = 1000;
constexpr std::size_t global_stack_size = 1000;

struct ThreadData {
    Stack* global_stack;
    int proc_count;
    int init_elems;
    int rank;
    bool is_main;
    double (*f)(double);
    double eps;
    double* sum;
};

void* thread_function(void* void_data) {
DOT
    const ThreadData& data = *reinterpret_cast<ThreadData*>(void_data);
    std::cout << "I am thread " << data.rank << std::endl;

    Stack& global_stack = *data.global_stack;
    Stack stack{local_stack_size};
DOT

    if (true) {
        for (int q = 0; q < data.init_elems; ++q) {
            Range range = global_stack.pop();
            stack.push(range);
        }
    }
    std::cout << "Stack of thread " << data.rank << std::endl;
    stack.print_stack();

    while (true) {
DOT
        if (!stack.is_empty()) {
DOT
            LOG("Proccesing range")
            Range cur_range = stack.pop();
            double sabc = cur_range.calc_area();
            double c = cur_range.calc_mid_point();
            double fc = data.f(c);
            if (!cur_range.calc_cond(data.eps, c, fc)) {    
                stack.push(cur_range.split_range(c, fc));
                stack.push(cur_range);
            } else {
                data.sum[data.rank] += sabc;
            }
            if (stack.get_occupancy() > stack.get_size()*2/3) {
                Stack::move_ratio(stack, global_stack, Stack::max_ratio);
            }
        } else {
            break;
            // if (data.is_main && global_stack.waiters_count() == data.proc_count) {
            //     // end the program
            // }
            // Stack::move_ratio_await(global_stack, stack, 30);
        }
std::cout << "\n\n" << std::endl;
// std::this_thread::sleep_for(std::chrono::seconds(1));
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

    Stack global_stack{global_stack_size};

    int cnt_per_proc = 10;
    int cnt = cnt_per_proc*proc_count;
    for (int q = 1; q < cnt; ++q) {
        double splitter = range.get_a() + (cnt-q) * range.get_len()/cnt;
        double value = f(splitter);
        global_stack.push(range.split_range(splitter, value));
    }
    global_stack.push(range);

    ThreadData* thread_data_arr = new ThreadData[proc_count];
    double* sum_arr = new double[proc_count];
    for (int q = 0; q < proc_count; ++q) {
        thread_data_arr[q] = {
            .global_stack = &global_stack,
            .proc_count = proc_count,
            .init_elems = cnt_per_proc,
            .rank = q,
            .is_main = (q == 0),
            .f = f,
            .eps = 0.001,
            .sum = sum_arr
        };
    }
	pthread_t* thread_arr = static_cast<pthread_t*>(::operator new[](sizeof(pthread_t)*proc_count));

        DOT
    for (int thread_num = 1; thread_num < proc_count; ++thread_num) {
        DOT
        pthread_create(
            &(thread_arr[thread_num]),
            NULL,
            thread_function,
            reinterpret_cast<void*>(&thread_data_arr[thread_num])
        );
    }
    thread_function(reinterpret_cast<void*>(&thread_data_arr[0]));

    for (int q = 0; q < proc_count; ++q) {
        pthread_join(thread_arr[q], nullptr);
    }

    double sum = 0;
    for (int q = 0; q < proc_count; ++q) {
        std::cout << sum_arr[q] << " ";
        sum += sum_arr[q];
    }
    std::cout << "\n sum: " << sum << std::endl;

    return 0;
}
