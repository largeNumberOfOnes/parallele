/**
 * This is realization of parallel integration algorithm on the system
 *     with shared memory.
 * For this program you can specify following macro words:
 *     + FUNCTION <expression with x variable>
 *     + INT_A <double>
 *     + INT_B <double>
 *     + EPS   <double>
 */

#include "integration_methods.h"
#include "range.h"
#include <chrono>
#include <cmath>



#ifndef FUNCTION
    #define FUNCTION x
#endif
#ifndef INT_A
    #define INT_A 0
#endif
#ifndef INT_B
    #define INT_B 1
#endif
#ifndef EPS
    #define EPS 0.001
#endif
#ifndef PROC
    #define PROC 5
#endif
#define STRINGIFY_NO_EXPAND(x) #x
#define STRINGIFY(x) STRINGIFY_NO_EXPAND(x)

double f(double x) {
    // return sin(1/x);
    return FUNCTION;
}

int main() {

    double a = INT_A;
    double b = INT_B;
    double eps = EPS; // eps = 0.00000001 sometimes works
    int proc_count = PROC;
    std::cout << "integrate " << STRINGIFY(FUNCTION) << " from " << a
        << " to " << b << std::endl;
    std::cout << "eps: " << eps << "; procs: " << proc_count << std::endl;
    
    auto start = std::chrono::steady_clock::now();
    
    double sum = global_stack_alg(
        Range{a, b, f(a), f(b)},
        f, eps, proc_count
    );

    auto end = std::chrono::steady_clock::now();
    auto duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "sum: " << sum << " calculated by " << duration.count()
        << " milliseconds" << std::endl;

    return 0;
}
