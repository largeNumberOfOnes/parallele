#pragma once

#include "range.h"
#include <boost/stacktrace/stacktrace_fwd.hpp>
#include <cstddef>
#include <cstdlib>
#include <semaphore.h>
#include <pthread.h>
#include <assert.h>
#include <iostream>
// #include <stacktrace>
#include <boost/stacktrace.hpp>

#include "../../slibs/err_proc.h"



class Stack {
    std::size_t size = 0;
    std::size_t ptr = 0;
    Range* range_arr = nullptr;

    public:
        inline Stack(std::size_t size);
        inline ~Stack();

        inline Range pop();
        inline void push(const Range& range);

        inline bool is_empty() const;
        inline bool is_full() const;
        inline std::size_t get_size();
        inline int get_occupancy() const;
        inline int get_free_space() const;

        inline static constexpr int max_ratio = 100;
        // Ration is in range [0, max ration]
        // Next functions return count of successfully moved elements
        static inline int move_elems_unsafe(Stack& from, Stack& to, int count);
        static inline int move_ratio(Stack& from, Stack& to, int ratio);
        static inline int move      (Stack& from, Stack& to, int count);

        inline void print_stack() const;
};

inline Stack::Stack(std::size_t size)
    : size(size)
    , ptr(0)
    , range_arr(
        reinterpret_cast<Range*>(::operator new (size * sizeof(Range)))
    )
{}

inline Stack::~Stack() {
    ::operator delete(range_arr);
}

inline Range Stack::pop() {
    assert(0 < ptr);
    Range ret = range_arr[--ptr];
    return ret;
}

inline void Stack::push(const Range& range) {
    assert(ptr < size);
    range_arr[ptr++] = range;
}

inline bool Stack::is_empty() const {
    return ptr == 0;
}

inline bool Stack::is_full() const {
    return ptr == size;
}

inline std::size_t Stack::get_size() {
    return size;
}

inline int Stack::get_occupancy() const {
    return ptr;
}

inline int Stack::get_free_space() const {
    return size - ptr;
}

inline int Stack::move_elems_unsafe(Stack& from, Stack& to, int count) {
    int ijected_count = 0;
    for (; ijected_count < count; ++ijected_count) {
        assert(from.ptr > 0);
        assert(to.ptr < to.size);
        --from.ptr;
        to.range_arr[to.ptr] = from.range_arr[from.ptr];
        ++to.ptr;
    }
    return ijected_count;
}

// inline int Stack::move_ratio(Stack& from, Stack& to, int ratio) {
//     int count = from.get_occupancy()*ratio/max_ratio;
//     int moved_count = move_elems_unsafe(from, to, count);
//     LockOnElements::send_unlock_signal(to);
//     return moved_count;
// }

// inline int Stack::move(Stack& from, Stack& to, int count) {
//     int count = std::min(
//         count, std::min(to.get_free_space(), from.get_occupancy())
//     );
//     int injected_count = move_elems_unsafe(from, to, count);
//     return injected_count;
// }

inline void Stack::print_stack() const {
    for (std::size_t q = 0; q < ptr; ++q) {
        std::cout << range_arr[q] << std::endl;
    }
}
