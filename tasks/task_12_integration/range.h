#pragma once

#include <cmath>
#include <ostream>




inline bool double_equal(double a, double b) {
    constexpr double eps = 0.0001;
    return fabs(a - b) < eps*(a + b);
}

class Range {
    double a;
    double b;
    double fa;
    double fb;

    public:
        inline Range(double a, double b, double fa, double fb);
        inline Range(double a, double b, double (*f)(double));
        inline Range(Range& range);

        inline bool is_valid() const;
        inline Range split_range(double splitter, double value);

        bool operator==(const Range& other) const;
        inline Range& operator=(const Range& other);

        double calc_area() const;
        double calc_mid_point() const;
        bool calc_cond(double eps, double c, double fc) const;

        inline friend std::ostream& operator<<(std::ostream& out,
                                                    const Range& range);

        inline double get_a() const { return a; }
        inline double get_b() const { return a; }
        inline double get_len() const { return b - a; }
};



#include "range.h"
#include <math.h>



inline Range::Range(double a, double b, double fa, double fb)
    : a(a), b(b), fa(fa), fb(fb)
{}

inline Range::Range(double a, double b, double (*f)(double))
    : a(a), b(b), fa(f(a)), fb(f(b))
{}

inline Range::Range(Range& range)
    : a (range.a)
    , b (range.b)
    , fa(range.fa)
    , fb(range.fb)
{}

// Cuts down given range to (a, spliter), returns (splitter, b)
inline Range Range::split_range(double splitter, double value) {
    Range ret = *this;
    ret.a  = splitter;
    ret.fa = value;
    b  = splitter;
    fb = value;
    return ret;
}

inline bool Range::is_valid() const {
    return a < b;
}

inline bool Range::operator==(const Range& other) const {
    return double_equal(a,  other.a)
        && double_equal(b,  other.b)
        && double_equal(fa, other.fa)
        && double_equal(fb, other.fb)
    ;
}

inline Range& Range::operator=(const Range& other) {
    a  = other.a;
    b  = other.b;
    fa = other.fa;
    fb = other.fb;
    return *this;
}

inline double Range::calc_area() const {
    return (fa + fb) * (b - a) / 2;
}

inline double Range::calc_mid_point() const {
    return (a + b) / 2;
}

inline bool Range::calc_cond(double eps, double c, double fc) const {
    double sAB = calc_area();
    double sACB = (fa + 2*fc + fb) * (b - a) / 4;
    return fabs(sAB - sACB) < eps*fabs(sACB);
}

inline std::ostream& operator<<(std::ostream& out, const Range& range) {
    out << "(" << range.a << " " << range.b << ")";
    return out;
}
