#include "integration_methods.h"
#include "range.h"
#include <cmath>

double f(double x) {
    return sin(1/x);
}

int main() {

    constexpr int proc_count = 5;
    double a = 0.01;
    double b = 1;
    global_stack_alg(
        Range{a, b, f(a), f(b)},
        f, proc_count
    );

    return 0;
}
