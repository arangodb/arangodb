/*
 * Copyright Nick Thompson, 2017
 * Use, modification and distribution are subject to the
 * Boost Software License, Version 1.0. (See accompanying file
 * LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
 */

#define BOOST_TEST_MODULE chebyshev_test

#include <boost/type_index.hpp>
#include <boost/test/included/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include <boost/math/special_functions/chebyshev.hpp>
#include <boost/math/special_functions/sinc.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/array.hpp>

using boost::multiprecision::cpp_bin_float_quad;
using boost::multiprecision::cpp_bin_float_50;
using boost::multiprecision::cpp_bin_float_100;
using boost::math::chebyshev_t;
using boost::math::chebyshev_t_prime;
using boost::math::chebyshev_u;

template<class Real>
void test_polynomials()
{
    std::cout << "Testing explicit polynomial representations of the Chebyshev polynomials on type " << boost::typeindex::type_id<Real>().pretty_name()  << "\n";

    Real x = -2;
    Real tol = 400*std::numeric_limits<Real>::epsilon();
    if (tol > std::numeric_limits<float>::epsilon())
       tol *= 10;   // float results have much larger error rates.
    while (x < 2)
    {
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(0, x), Real(1), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(1, x), x, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(2, x), 2*x*x - 1, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(3, x), x*(4*x*x-3), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(4, x), 8*x*x*(x*x - 1) + 1, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t(5, x), x*(16*x*x*x*x - 20*x*x + 5), tol);
        x += 1/static_cast<Real>(1<<7);
    }

    x = -2;
    tol = 10*tol;
    while (x < 2)
    {
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_u(0, x), Real(1), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_u(1, x), 2*x, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_u(2, x), 4*x*x - 1, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_u(3, x), 4*x*(2*x*x - 1), tol);
        x += 1/static_cast<Real>(1<<7);
    }
}


template<class Real>
void test_derivatives()
{
    std::cout << "Testing explicit polynomial representations of the Chebyshev polynomial derivatives on type " << boost::typeindex::type_id<Real>().pretty_name()  << "\n";

    Real x = -2;
    Real tol = 1000*std::numeric_limits<Real>::epsilon();
    while (x < 2)
    {
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(0, x), Real(0), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(1, x), Real(1), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(2, x), 4*x, tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(3, x), 3*(4*x*x - 1), tol);
        BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(4, x), 16*x*(2*x*x - 1), tol);
        // This one makes the tolerance have to grow too large; the Chebyshev recurrence is more stable than naive polynomial evaluation anyway.
        //BOOST_CHECK_CLOSE_FRACTION(chebyshev_t_prime(5, x), 5*(4*x*x*(4*x*x - 3) + 1), tol);
        x += 1/static_cast<Real>(1<<7);
    }
}

template<class Real>
void test_clenshaw_recurrence()
{
    using boost::math::chebyshev_clenshaw_recurrence;
    boost::array<Real, 5> c0 = { {2, 0, 0, 0, 0} };
    // Check the size = 1 case:
    boost::array<Real, 1> c01 = { {2} };
    // Check the size = 2 case:
    boost::array<Real, 2> c02 = { {2, 0} };
    boost::array<Real, 4> c1 = { {0, 1, 0, 0} };
    boost::array<Real, 4> c2 = { {0, 0, 1, 0} };
    boost::array<Real, 5> c3 = { {0, 0, 0, 1, 0} };
    boost::array<Real, 5> c4 = { {0, 0, 0, 0, 1} };
    boost::array<Real, 6> c5 = { {0, 0, 0, 0, 0, 1} };
    boost::array<Real, 7> c6 = { {0, 0, 0, 0, 0, 0, 1} };

    Real x = -1;
    Real tol = 10*std::numeric_limits<Real>::epsilon();
    if (tol > std::numeric_limits<float>::epsilon())
       tol *= 100;   // float results have much larger error rates.
    while (x <= 1)
    {
        Real y = chebyshev_clenshaw_recurrence(c0.data(), c0.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(0, x), tol);

        y = chebyshev_clenshaw_recurrence(c01.data(), c01.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(0, x), tol);

        y = chebyshev_clenshaw_recurrence(c02.data(), c02.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(0, x), tol);

        y = chebyshev_clenshaw_recurrence(c1.data(), c1.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(1, x), tol);

        y = chebyshev_clenshaw_recurrence(c2.data(), c2.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(2, x), tol);

        y = chebyshev_clenshaw_recurrence(c3.data(), c3.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(3, x), tol);

        y = chebyshev_clenshaw_recurrence(c4.data(), c4.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(4, x), tol);

        y = chebyshev_clenshaw_recurrence(c5.data(), c5.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(5, x), tol);

        y = chebyshev_clenshaw_recurrence(c6.data(), c6.size(), x);
        BOOST_CHECK_CLOSE_FRACTION(y, chebyshev_t(6, x), tol);

        x += static_cast<Real>(1)/static_cast<Real>(1 << 7);
    }
}

BOOST_AUTO_TEST_CASE(chebyshev_test)
{
    test_clenshaw_recurrence<float>();
    test_clenshaw_recurrence<double>();
    test_clenshaw_recurrence<long double>();

    test_polynomials<float>();
    test_polynomials<double>();
    test_polynomials<long double>();
    test_polynomials<cpp_bin_float_quad>();

    test_derivatives<float>();
    test_derivatives<double>();
    test_derivatives<long double>();
    test_derivatives<cpp_bin_float_quad>();
}
