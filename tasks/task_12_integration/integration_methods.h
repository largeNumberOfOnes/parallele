#include "range.h"




double local_stack_alg(double a, double b, double (*f)(double));


double global_stack_alg(
    Range range,
    double (*f)(double),
    double eps,
    int proc_count
);
