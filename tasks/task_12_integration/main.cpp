#include "integration_methods.h"
#include "range.h"
#include <cmath>

double f(double x) {
    return sin(1/x);
}

int main() {

    constexpr int proc_count = 5;
    double a = 0.0001;
    double b = 1;
    double eps = 0.00000001;
    // double eps = 0.000000001;
    std::cout << "eps " << eps << std::endl;
    global_stack_alg(
        Range{a, b, f(a), f(b)},
        f, eps, proc_count
    );

    return 0;
}
