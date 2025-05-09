#pragma once

#include "range.h"
#include <cstddef>
#include <semaphore.h>
#include <pthread.h>
#include <assert.h>
#include <iostream>

#include "../../slibs/err_proc.h"



class Stack {
    std::size_t size = 0;
    std::size_t ptr = 0;
    Range* range_arr = nullptr;

    const bool THREAD_SAFE;

    mutable pthread_mutex_t mutex_lock = PTHREAD_MUTEX_INITIALIZER;
    class LockOnAccess {
        const Stack& stack;
        public:
            LockOnAccess(const Stack& stack) : stack(stack) {
                if (stack.THREAD_SAFE) {
                    pthread_mutex_lock(&stack.mutex_lock);
                    LOG("lock_access stack [%p]", &stack)
                }
            }
            ~LockOnAccess(){
                if (stack.THREAD_SAFE) {
                    pthread_mutex_unlock(&stack.mutex_lock);
                    LOG("unlock_access stack [%p]", &stack)
                }
            }
    };

    mutable pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
    mutable pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    mutable int waiters_count = 0;
    class LockOnElements {
        const Stack& stack;
        public:
            static bool condition(const Stack& stack) {
                return stack.ptr > 0;
            }
            LockOnElements(const Stack& stack) : stack(stack) {
                if (stack.THREAD_SAFE) {
                    pthread_mutex_lock(&stack.cond_mutex);
                    stack.waiters_count += 1;
                    LOG("lock_elem stack [%p]", &stack)
                    while (!condition(stack)) {
                        pthread_cond_wait(&stack.cond, &stack.cond_mutex);
                    }
                    stack.waiters_count -= 1;
                }
            }
            static void send_unlock_signal(Stack& stack) {
                pthread_cond_signal(&stack.cond);
                LOG("sending unlock_elem signal for stack [%p]", &stack)
            }
            ~LockOnElements() {
                if (stack.THREAD_SAFE) {
                    LOG("unlock_elem stack [%p]", &stack)
                    pthread_mutex_unlock(&stack.cond_mutex);
                }
            }
    };

    public:
        inline Stack(std::size_t size, bool thread_safe);
        inline ~Stack();

        inline Range pop();
        inline void push(const Range& range);

        inline bool is_empty() const;
        inline bool is_full() const;
        inline std::size_t get_size();
        inline int get_occupancy() const;
        inline int get_free_space() const;
        inline int get_waiters_count() const;

        inline static constexpr int max_ratio = 100;
        // Ration is in range [0, max ration]
        // Next functions return count of successfully moved elements
        static inline int move_elems_unsafe(Stack& from, Stack& to, int count);
        static inline int move_ratio(Stack& from, Stack& to, int ratio);
        static inline int move      (Stack& from, Stack& to, int count);

        inline void wait_waiters_count(int count) const;

        inline void print_stack() const;
};

inline Stack::Stack(std::size_t size, bool thread_safe = true)
    : size(size)
    , ptr(0)
    , range_arr(
        reinterpret_cast<Range*>(::operator new (size * sizeof(Range)))
    )
    , THREAD_SAFE(thread_safe)
{}

inline Stack::~Stack() {
    ::operator delete(range_arr);
}

inline Range Stack::pop() {
    LockOnElements lock1(*this);
    LockOnAccess lock2(*this);
    assert(0 < ptr);
    // std::cout << "poping " << range_arr[ptr-1] << std::endl;
    Range ret = range_arr[--ptr];
    return ret;
}

inline void Stack::push(const Range& range) {
    assert(ptr < size);
    LockOnAccess lock1(*this);
    // std::cout << "pushing " << range << std::endl;
    range_arr[ptr++] = range;
    LockOnElements::send_unlock_signal(*this);
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
inline int Stack::get_waiters_count() const {
    return waiters_count;
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

inline int Stack::move_ratio(Stack& from, Stack& to, int ratio) {
    LockOnElements lock1{from};
    LockOnAccess   lock2{from};
    LockOnAccess   lock3{to};
    int count = from.get_occupancy()*ratio/max_ratio;
    int moved_count = move_elems_unsafe(from, to, count);
    LockOnElements::send_unlock_signal(to);
    return moved_count;
}

inline int Stack::move(Stack& from, Stack& to, int ratio) {
    LockOnElements lock1{from};
    LockOnAccess   lock2{from};
    LockOnAccess   lock3{to};
    int count = std::min(
        count, std::min(to.get_free_space(), from.get_occupancy())
    );
    int injected_count = move_elems_unsafe(from, to, count);
    return injected_count;
}

inline void Stack::wait_waiters_count(int count) const {
    while (get_waiters_count() != count - 1) {
        LockOnElements lock1(*this);
        lock1.~LockOnElements();
    }
}

inline void Stack::print_stack() const {
    for (int q = 0; q < ptr; ++q) {
        std::cout << range_arr[q] << std::endl;
    }
}
