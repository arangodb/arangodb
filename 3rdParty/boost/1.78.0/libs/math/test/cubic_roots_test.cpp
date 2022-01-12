/*
 * Copyright Nick Thompson, 2021
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#include "math_unit_test.hpp"
#include <random>
#include <boost/math/tools/cubic_roots.hpp>
#ifdef BOOST_HAS_FLOAT128
#include <boost/multiprecision/float128.hpp>
using boost::multiprecision::float128;
#endif

using boost::math::tools::cubic_roots;
using boost::math::tools::cubic_root_residual;
using std::cbrt;

template<class Real>
void test_zero_coefficients()
{
    Real a = 0;
    Real b = 0;
    Real c = 0;
    Real d = 0;
    auto roots = cubic_roots(a,b,c,d);
    CHECK_EQUAL(roots[0], Real(0));
    CHECK_EQUAL(roots[1], Real(0));
    CHECK_EQUAL(roots[2], Real(0));

    a = 1;
    roots = cubic_roots(a,b,c,d);
    CHECK_EQUAL(roots[0], Real(0));
    CHECK_EQUAL(roots[1], Real(0));
    CHECK_EQUAL(roots[2], Real(0));

    a = 1;
    d = 1;
    // x^3 + 1 = 0:
    roots = cubic_roots(a,b,c,d);
    CHECK_EQUAL(roots[0], Real(-1));
    CHECK_NAN(roots[1]);
    CHECK_NAN(roots[2]);
    d = -1;
    // x^3 - 1 = 0:
    roots = cubic_roots(a,b,c,d);
    CHECK_EQUAL(roots[0], Real(1));
    CHECK_NAN(roots[1]);
    CHECK_NAN(roots[2]);

    d = -2;
    // x^3 - 2 = 0
    roots = cubic_roots(a,b,c,d);
    CHECK_ULP_CLOSE(roots[0], cbrt(Real(2)), 2);
    CHECK_NAN(roots[1]);
    CHECK_NAN(roots[2]);

    d = -8;
    roots = cubic_roots(a,b,c,d);
    CHECK_EQUAL(roots[0], Real(2));
    CHECK_NAN(roots[1]);
    CHECK_NAN(roots[2]);


    // (x-1)(x-2)(x-3) = x^3 - 6x^2 + 11x - 6
    roots = cubic_roots(Real(1), Real(-6), Real(11), Real(-6));
    CHECK_ULP_CLOSE(roots[0], Real(1), 2);
    CHECK_ULP_CLOSE(roots[1], Real(2), 2);
    CHECK_ULP_CLOSE(roots[2], Real(3), 2);

    // Double root:
    // (x+1)^2(x-2) = x^3 - 3x - 2:
    // Note: This test is unstable wrt to perturbations!
    roots = cubic_roots(Real(1), Real(0), Real(-3), Real(-2));
    CHECK_ULP_CLOSE(Real(-1), roots[0], 2);
    CHECK_ULP_CLOSE(Real(-1), roots[1], 2);
    CHECK_ULP_CLOSE(Real(2), roots[2], 2);

    std::uniform_real_distribution<Real> dis(-2,2);
    std::mt19937 gen(12345);
    // Expected roots
    std::array<Real, 3> r;
    int trials = 10;
    for (int i = 0; i < trials; ++i) {
        // Mathematica:
        // Expand[(x - r0)*(x - r1)*(x - r2)]
        // - r0 r1 r2 + (r0 r1 + r0 r2 + r1 r2) x
        // - (r0 + r1 + r2) x^2 + x^3
        for (auto & root : r) {
            root = static_cast<Real>(dis(gen));
        }
        std::sort(r.begin(), r.end());
        Real a = 1;
        Real b = -(r[0] + r[1] + r[2]);
        Real c = r[0]*r[1] + r[0]*r[2] + r[1]*r[2];
        Real d = -r[0]*r[1]*r[2];

        auto roots = cubic_roots(a, b, c, d);
        // I could check the condition number here, but this is fine right?
        if(!CHECK_ULP_CLOSE(r[0], roots[0], 25)) {
            std::cerr << "  Polynomial x^3 + " << b << "x^2 + " << c << "x + " << d << " has roots {";
            std::cerr << r[0] << ", " << r[1] << ", " << r[2] << "}, but the computed roots are {";
            std::cerr << roots[0] << ", " << roots[1] << ", " << roots[2] << "}\n";
        }
        CHECK_ULP_CLOSE(r[1], roots[1], 25);
        CHECK_ULP_CLOSE(r[2], roots[2], 25);
        for (auto root : roots) {
            auto res = cubic_root_residual(a, b,c,d, root);
            CHECK_LE(abs(res[0]), 20*res[1]);
        }
    }
}


int main()
{
    test_zero_coefficients<float>();
    test_zero_coefficients<double>();
#ifndef BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS
    test_zero_coefficients<long double>();
#endif

#ifdef BOOST_HAS_FLOAT128
    // For some reason, the quadmath is way less accurate than the float/double/long double:
    //test_zero_coefficients<float128>();
#endif


    return boost::math::test::report_errors();
}
