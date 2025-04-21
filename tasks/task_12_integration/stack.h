#pragma once

#include "range.h"
#include <cstddef>
#include <semaphore.h>
#include <pthread.h>
#include <assert.h>
#include <iostream>

#include "../../slibs/err_proc.h"


// template <bool THREAD_SAFE = true>
class Stack {
    std::size_t size = 0;
    std::size_t ptr = 0;
    Range* range_arr = nullptr;

    mutable pthread_mutex_t mutex; 
    mutable pthread_mutex_t mutex_waiters;
    pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
    // sem_t semaphore;
    mutable int waiters_count = 0;

    class LockObject {
        const Stack& stack;
        public:
            LockObject(const Stack& stack) : stack(stack) {
                pthread_mutex_lock(&stack.mutex);
            }
            ~LockObject(){
                pthread_mutex_unlock(&stack.mutex);
            }
    };

    inline void block_access();
    inline void unblock_access();
    inline void wait_unblocked_access();

    friend inline int move_elems_unsafe(Stack& from, Stack& to, int count);

    public:
        inline Stack(size_t size);
        inline ~Stack();
        inline bool is_empty() const;
        inline Range pop();
        inline void push(const Range& range);

        static constexpr int max_ratio = 100;
        // Ration is in range [0, max ration]
        // Next functions return count of successfully moved elements
        static inline int move_ratio(Stack& from, Stack& to, int ratio);
        static inline int move      (Stack& from, Stack& to, int count);

        inline std::size_t get_size() {
            return size;
        }

        inline bool is_full() const;
        inline int get_occupancy() const;
        inline int get_free_space() const;

        inline void print_stack() const;
};

inline void Stack::block_access() {
    pthread_mutex_lock(&mutex_waiters);
}

inline void Stack::unblock_access() {
    pthread_mutex_unlock(&mutex_waiters);
}
inline void Stack::wait_unblocked_access() {

}

void lock_on_elements() {

}



inline Stack::Stack(std::size_t size)
    : size(size)
    , ptr(0)
    , range_arr(reinterpret_cast<Range*>(::operator new (size*sizeof(Range))))
{
    if (pthread_mutex_init(&mutex, nullptr) != 0) { 
        printf("\n mutex init has failed\n");
        assert(false);
    }
    // cond = PTHREAD_MUTEX_INITIALIZER;
    int sem_init(sem_t *sem, int pshared, unsigned int value);
}

inline Stack::~Stack() {
    ::operator delete(range_arr);
}

inline Range Stack::pop() {
    assert(0 < ptr);
    // lock_on_elements();
    // LockObject obj{*this};
    pthread_mutex_lock(&mutex);
    while (ptr == 0) {
        pthread_cond_wait(&cond, &mutex);
    }
    std::cout << "poping " << range_arr[ptr-1] << std::endl;
    Range ret = range_arr[--ptr];
    pthread_mutex_unlock(&mutex);
    return ret;
}

inline void Stack::push(const Range& range) {
    assert(ptr < size);
    pthread_mutex_lock(&mutex);
    std::cout << "pushing " << range << std::endl;
    range_arr[ptr++] = range;
    pthread_cond_signal(&cond);
    pthread_mutex_unlock(&mutex);
}

inline bool Stack::is_empty() const {
    return ptr == 0;
}

inline bool Stack::is_full() const {
    return ptr == size;
}

inline int Stack::get_occupancy() const {
    return ptr;
}

inline int Stack::get_free_space() const {
    return size - ptr;
}

inline int move_elems_unsafe(Stack& from, Stack& to, int count) {
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
    LockObject obj1{from};
    LockObject obj2{to};
    int count = from.get_occupancy()*ratio/max_ratio;
    int moved_count = move_elems_unsafe(from, to, count);
    return moved_count;
}

inline int Stack::move(Stack& from, Stack& to, int ratio) {
    LockObject obj1{from};
    LockObject obj2{to};
    int count = std::min(
        count, std::min(to.get_free_space(), from.get_occupancy())
    );
    int injected_count = move_elems_unsafe(from, to, count);
    return injected_count;
}

inline void Stack::print_stack() const {
    for (int q = 0; q < ptr; ++q) {
        std::cout << range_arr[q] << std::endl;
    }
}
