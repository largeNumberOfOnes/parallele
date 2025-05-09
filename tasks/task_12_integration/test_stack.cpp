#include "range.h"
#include "stack.h"
#include <cassert>
#include <chrono>
#include <pthread.h>
#include <thread>



void test_stack_1() {
    Stack stack{1000};
}

void test_stack_2() {
    Range range1{0, 1, 0, 0};
    Range range2{0, 2, 0, 0};
    Range range3{0, 3, 0, 0};
    Range range4{0, 4, 0, 0};

    Stack stack{1000};

    stack.push(range1);
    stack.push(range2);
    stack.push(range3);
    stack.push(range4);
    // std::cout << stack.pop() << std::endl;
    assert(stack.pop() == range4);
    assert(stack.pop() == range3);
}

struct test_stack_3_thread_data {
    Stack* stack;
};

void* test_stack_3_thread_function(void* data) {
    Range range4{0, 4, 0, 0};
    std::this_thread::sleep_for(std::chrono::seconds(2));
    std::cout << "starting new thread" << std::endl;
    reinterpret_cast<test_stack_3_thread_data*>(data)->stack->push(range4);
    return nullptr;
}

void test_stack_3() {
    Range range1{0, 1, 0, 0};
    Range range2{0, 2, 0, 0};
    Range range3{0, 3, 0, 0};
    Range range4{0, 4, 0, 0};

    Stack stack{1000};

    stack.push(range1);
    stack.push(range2);
    assert(stack.pop() == range2);
    assert(stack.pop() == range1);

    test_stack_3_thread_data data {
        .stack = &stack
    };

    pthread_t newthread;
    pthread_create(
        &newthread,
        NULL,
        test_stack_3_thread_function,
        &data
    );
    std::cout << "try pop" << std::endl;
    assert(stack.pop() == range4);
    std::cout << "try pop succeed" << std::endl;

    stack.push(range3);
    assert(stack.pop() == range3);
    pthread_join(newthread, nullptr);
}

void test_stack() {

    test_stack_1();
    test_stack_2();
    test_stack_3();

}

int main() {
    test_stack();

    return 0;
}
