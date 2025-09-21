#pragma once

#include "range.h"
#include <cstring>
#include <assert.h>



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

        static inline void move(Stack& from, Stack& to, int count);

        inline void print_stack() const;
};

inline Stack::Stack(std::size_t size)
    : size(size)
    , ptr(0)
    , range_arr(
        reinterpret_cast<Range*>(::operator new (size * sizeof(Range)))
        // new Range[size]
    )
{}

inline Stack::~Stack() {
    ::operator delete(range_arr);
    // delete [] range_arr;
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

inline void Stack::move(Stack& from, Stack& to, int count) {
    assert(from.ptr >= static_cast<std::size_t>(count));
    assert(to.ptr + count < to.size);
    std::memcpy(
        reinterpret_cast<void*>(to.range_arr + to.ptr),
        reinterpret_cast<void*>(from.range_arr + from.ptr - count),
        count * sizeof(Range)
    );
    to.ptr   += count;
    from.ptr -= count;
}

inline void Stack::print_stack() const {
    for (std::size_t q = 0; q < ptr; ++q) {
        std::cout << range_arr[q] << std::endl;
    }
}
