#pragma once

#include <cmath>
#include <iostream>
#include <ostream>



static inline bool double_equal(double a, double b) {
    constexpr double eps = 0.0001;
    return fabs(a - b) < eps;
}

class Range {
    double a;
    double b;
    double fa;
    double fb;

    public:
        inline Range();
        inline Range(double a, double b, double fa, double fb);
        inline Range(double a, double b, double (*f)(double));

        inline bool is_valid() const;
        inline Range split_range(double splitter, double value);

        bool operator==(const Range& other) const;

        double calc_area() const;
        double calc_mid_point() const;
        bool calc_cond(double eps, double c, double fc) const;

        inline friend std::ostream& operator<<(std::ostream& out,
                                                    const Range& range);

        inline double get_a() const { return a; }
        inline double get_b() const { return a; }
        inline double get_len() const { return b - a; }
};

inline Range::Range()
    : a(0), b(0), fa(0), fb(0)
{}

inline Range::Range(double a, double b, double fa, double fb)
    : a(a), b(b), fa(fa), fb(fb)
{}

inline Range::Range(double a, double b, double (*f)(double))
    : a(a), b(b), fa(f(a)), fb(f(b))
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
    double eps = 1e-14;
    double c = calc_mid_point();
    return a + eps < c && c + eps < b && a + eps < b;
}

inline bool Range::operator==(const Range& other) const {
    return double_equal(a ,  other.a)
        && double_equal(b ,  other.b)
        && double_equal(fa, other.fa)
        && double_equal(fb, other.fb)
    ;
}

inline double Range::calc_area() const {
    return (fa + fb) * (b - a) / 2;
}

inline double Range::calc_mid_point() const {
    return (a + b) / 2;
}

inline bool Range::calc_cond(double eps, double c, double fc) const {
    double sAB = calc_area();
    double sACB = (c*fa - a*fc + b*fc - c*fb + b*fb - a*fa)/2;
    return fabs(sAB - sACB) < eps*fabs(sACB);
}

inline std::ostream& operator<<(std::ostream& out, const Range& range) {
    out << "(" << range.a << " " << range.b << ")" << range.b - range.a;
    return out;
}
