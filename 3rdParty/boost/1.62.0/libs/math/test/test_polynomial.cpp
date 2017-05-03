//  (C) Copyright Jeremy Murphy 2015.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#define BOOST_TEST_MAIN
#include <boost/array.hpp>
#include <boost/math/tools/polynomial.hpp>
#include <boost/math/common_factor_rt.hpp>
#include <boost/mpl/list.hpp>
#include <boost/mpl/joint_view.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <utility>

using namespace boost::math::tools;
using namespace std;

template <typename T>
struct answer
{
    answer(std::pair< polynomial<T>, polynomial<T> > const &x) :
    quotient(x.first), remainder(x.second) {}
    
    polynomial<T> quotient;
    polynomial<T> remainder;
};

boost::array<double, 4> const d3a = {{10, -6, -4, 3}};
boost::array<double, 4> const d3b = {{-7, 5, 6, 1}};
boost::array<double, 4> const d3c = {{10.0/3.0, -2.0, -4.0/3.0, 1.0}};
boost::array<double, 2> const d1a = {{-2, 1}};
boost::array<double, 3> const d2a = {{-2, 2, 3}};
boost::array<double, 3> const d2b = {{-7, 5, 6}};
boost::array<double, 3> const d2c = {{31, -21, -22}};
boost::array<double, 1> const d0a = {{6}};
boost::array<double, 2> const d0a1 = {{0, 6}};
boost::array<double, 6> const d0a5 = {{0, 0, 0, 0, 0, 6}};
boost::array<double, 1> const d0b = {{3}};

boost::array<int, 9> const d8 = {{-5, 2, 8, -3, -3, 0, 1, 0, 1}};
boost::array<int, 9> const d8b = {{0, 2, 8, -3, -3, 0, 1, 0, 1}};
boost::array<int, 7> const d6 = {{21, -9, -4, 0, 5, 0, 3}};
boost::array<int, 3> const d2 = {{-6, 0, 9}};
boost::array<int, 6> const d5 = {{-9, 0, 3, 0, -15}};


BOOST_AUTO_TEST_CASE( test_construction )
{
    polynomial<double> const a(d3a.begin(), d3a.end());
    polynomial<double> const b(d3a.begin(), 3);
    BOOST_CHECK_EQUAL(a, b);
}


#if !defined(BOOST_NO_CXX11_HDR_INITIALIZER_LIST) && !BOOST_WORKAROUND(BOOST_GCC_VERSION, < 40500)
BOOST_AUTO_TEST_CASE( test_initializer_list_construction )
{
    polynomial<double> a(begin(d3a), end(d3a));
    polynomial<double> b = {10, -6, -4, 3};
    polynomial<double> c{{10, -6, -4, 3}};
    polynomial<double> d{{10, -6, -4, 3, 0, 0}};
    BOOST_CHECK_EQUAL(a, b);
    BOOST_CHECK_EQUAL(b, c);
    BOOST_CHECK_EQUAL(d.degree(), 3u);
}

BOOST_AUTO_TEST_CASE( test_initializer_list_assignment )
{
    polynomial<double> a(begin(d3a), end(d3a));
    polynomial<double> b;
    b = {10, -6, -4, 3, 0, 0};
    BOOST_CHECK_EQUAL(b.degree(), 3u);
    BOOST_CHECK_EQUAL(a, b);
}
#endif


BOOST_AUTO_TEST_CASE( test_degree )
{
    polynomial<double> const zero;
    polynomial<double> const a(d3a.begin(), d3a.end());
    BOOST_CHECK_THROW(zero.degree(), std::logic_error);
    BOOST_CHECK_EQUAL(a.degree(), 3u);
}


BOOST_AUTO_TEST_CASE( test_division_over_field )
{
    polynomial<double> const a(d3a.begin(), d3a.end());
    polynomial<double> const b(d1a.begin(), d1a.end());
    polynomial<double> const q(d2a.begin(), d2a.end());
    polynomial<double> const r(d0a.begin(), d0a.end());
    polynomial<double> const c(d3b.begin(), d3b.end());
    polynomial<double> const d(d2b.begin(), d2b.end());
    polynomial<double> const e(d2c.begin(), d2c.end());
    polynomial<double> const f(d0b.begin(), d0b.end());
    polynomial<double> const g(d3c.begin(), d3c.end());
    polynomial<double> const zero;
    polynomial<double> const one(1.0);

    answer<double> result = quotient_remainder(a, b);
    BOOST_CHECK_EQUAL(result.quotient, q);
    BOOST_CHECK_EQUAL(result.remainder, r);
    BOOST_CHECK_EQUAL(a, q * b + r); // Sanity check.
    
    result = quotient_remainder(a, c);
    BOOST_CHECK_EQUAL(result.quotient, f);
    BOOST_CHECK_EQUAL(result.remainder, e);
    BOOST_CHECK_EQUAL(a, f * c + e); // Sanity check.
    
    result = quotient_remainder(a, f);
    BOOST_CHECK_EQUAL(result.quotient, g);
    BOOST_CHECK_EQUAL(result.remainder, zero);
    BOOST_CHECK_EQUAL(a, g * f + zero); // Sanity check.
    // Check that division by a regular number gives the same result.
    BOOST_CHECK_EQUAL(a / 3.0, g);
    BOOST_CHECK_EQUAL(a % 3.0, zero);

    // Sanity checks.
    BOOST_CHECK_EQUAL(a / a, one);
    BOOST_CHECK_EQUAL(a % a, zero);
    // BOOST_CHECK_EQUAL(zero / zero, zero); // TODO
}

BOOST_AUTO_TEST_CASE( test_division_over_ufd )
{
    polynomial<int> const zero;
    polynomial<int> const one(1);
    polynomial<int> const aa(d8.begin(), d8.end());
    polynomial<int> const bb(d6.begin(), d6.end());
    polynomial<int> const q(d2.begin(), d2.end());
    polynomial<int> const r(d5.begin(), d5.end());
    
    answer<int> result = quotient_remainder(aa, bb);
    BOOST_CHECK_EQUAL(result.quotient, q);
    BOOST_CHECK_EQUAL(result.remainder, r);

    // Sanity checks.
    BOOST_CHECK_EQUAL(aa / aa, one);
    BOOST_CHECK_EQUAL(aa % aa, zero);
}


BOOST_AUTO_TEST_CASE( test_gcd )
{
    /* NOTE: Euclidean gcd is not yet customized to return THE greatest 
     * common polynomial divisor. If d is THE greatest common divisior of u and
     * v, then gcd(u, v) will return d or -d according to the algorithm.
     * By convention, it should return d, as for example Maxima and Wolfram 
     * Alpha do.
     * This test is an example of the fact that it returns -d.
     */
    boost::array<double, 9> const d8 = {{105, 278, -88, -56, 16}};
    boost::array<double, 7> const d6 = {{70, 232, -44, -64, 16}};
    boost::array<double, 7> const d2 = {{-35, 24, -4}};
    polynomial<double> const u(d8.begin(), d8.end());
    polynomial<double> const v(d6.begin(), d6.end());
    polynomial<double> const w(d2.begin(), d2.end());
    polynomial<double> const d = boost::math::gcd(u, v);
    BOOST_CHECK_EQUAL(w, d);
}

// Sanity checks to make sure I didn't break it.
typedef boost::mpl::list<int, long
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
   , boost::multiprecision::cpp_int
#endif
> integral_test_types;
typedef boost::mpl::list<double
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
   , boost::multiprecision::cpp_rational, boost::multiprecision::cpp_bin_float_single, boost::multiprecision::cpp_dec_float_50
#endif
> non_integral_test_types;
typedef boost::mpl::joint_view<integral_test_types, non_integral_test_types> all_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE( test_addition, T, all_test_types )
{
    polynomial<T> const a(d3a.begin(), d3a.end());
    polynomial<T> const b(d1a.begin(), d1a.end());
    polynomial<T> const zero;
    
    polynomial<T> result = a + b; // different degree
    boost::array<T, 4> tmp = {{8, -5, -4, 3}};
    polynomial<T> expected(tmp.begin(), tmp.end());
    BOOST_CHECK_EQUAL(result, expected);
    BOOST_CHECK_EQUAL(a + zero, a);
    BOOST_CHECK_EQUAL(a + b, b + a);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_subtraction, T, all_test_types )
{
    polynomial<T> const a(d3a.begin(), d3a.end());
    polynomial<T> const zero;

    BOOST_CHECK_EQUAL(a - T(0), a);
    BOOST_CHECK_EQUAL(T(0) - a, -a);
    BOOST_CHECK_EQUAL(a - zero, a);
    BOOST_CHECK_EQUAL(zero - a, -a);
    BOOST_CHECK_EQUAL(a - a, zero);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_multiplication, T, all_test_types )
{
    polynomial<T> const a(d3a.begin(), d3a.end());
    polynomial<T> const b(d1a.begin(), d1a.end());
    polynomial<T> const zero;
    boost::array<T, 7> const d3a_sq = {{100, -120, -44, 108, -20, -24, 9}};
    polynomial<T> const a_sq(d3a_sq.begin(), d3a_sq.end());
    
    BOOST_CHECK_EQUAL(a * T(0), zero);
    BOOST_CHECK_EQUAL(a * zero, zero);
    BOOST_CHECK_EQUAL(zero * T(0), zero);
    BOOST_CHECK_EQUAL(zero * zero, zero);
    BOOST_CHECK_EQUAL(a * b, b * a);
    polynomial<T> aa(a);
    aa *= aa;
    BOOST_CHECK_EQUAL(aa, a_sq);
    BOOST_CHECK_EQUAL(aa, a * a);
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_arithmetic_relations, T, all_test_types )
{
    polynomial<T> const a(d8b.begin(), d8b.end());
    polynomial<T> const b(d1a.begin(), d1a.end());

    BOOST_CHECK_EQUAL(a * T(2), a + a);
    BOOST_CHECK_EQUAL(a - b, -b + a);
    BOOST_CHECK_EQUAL(a, (a * a) / a);
    BOOST_CHECK_EQUAL(a, (a / a) * a);
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_non_integral_arithmetic_relations, T, non_integral_test_types )
{
    polynomial<T> const a(d8b.begin(), d8b.end());
    polynomial<T> const b(d1a.begin(), d1a.end());
    
    BOOST_CHECK_EQUAL(a * T(0.5), a / T(2));
}

BOOST_AUTO_TEST_CASE_TEMPLATE( test_self_multiply_assign, T, all_test_types )
{
    polynomial<T> a(d3a.begin(), d3a.end());
    polynomial<T> const b(a);
    boost::array<double, 7> const d3a_sq = {{100, -120, -44, 108, -20, -24, 9}};
    polynomial<T> const asq(d3a_sq.begin(), d3a_sq.end());

    a *= a;

    BOOST_CHECK_EQUAL(a, b*b);
    BOOST_CHECK_EQUAL(a, asq);

    a *= a;

    BOOST_CHECK_EQUAL(a, b*b*b*b);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_right_shift, T, all_test_types )
{
    polynomial<T> a(d8b.begin(), d8b.end());
    polynomial<T> const aa(a);
    polynomial<T> const b(d8b.begin() + 1, d8b.end());
    polynomial<T> const c(d8b.begin() + 5, d8b.end());
    a >>= 0u;
    BOOST_CHECK_EQUAL(a, aa);
    a >>= 1u;
    BOOST_CHECK_EQUAL(a, b);
    a = a >> 4u;
    BOOST_CHECK_EQUAL(a, c);
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_left_shift, T, all_test_types )
{
    polynomial<T> a(d0a.begin(), d0a.end());
    polynomial<T> const aa(a);
    polynomial<T> const b(d0a1.begin(), d0a1.end());
    polynomial<T> const c(d0a5.begin(), d0a5.end());
    a <<= 0u;
    BOOST_CHECK_EQUAL(a, aa);    
    a <<= 1u;
    BOOST_CHECK_EQUAL(a, b);
    a = a << 4u;
    BOOST_CHECK_EQUAL(a, c);
    polynomial<T> zero;
    // Multiplying zero by x should still be zero.
    zero <<= 1u;
    BOOST_CHECK_EQUAL(zero, zero_element(multiplies< polynomial<T> >()));
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_odd_even, T, all_test_types)
{
    polynomial<T> const zero;
    BOOST_CHECK_EQUAL(odd(zero), false);
    BOOST_CHECK_EQUAL(even(zero), true);
    polynomial<T> const a(d0a.begin(), d0a.end());
    BOOST_CHECK_EQUAL(odd(a), true);
    BOOST_CHECK_EQUAL(even(a), false);
    polynomial<T> const b(d0a1.begin(), d0a1.end());
    BOOST_CHECK_EQUAL(odd(b), false);
    BOOST_CHECK_EQUAL(even(b), true);
}


BOOST_AUTO_TEST_CASE_TEMPLATE( test_pow, T, all_test_types )
{
    polynomial<T> a(d3a.begin(), d3a.end());
    polynomial<T> const one(T(1));
    boost::array<double, 7> const d3a_sqr = {{100, -120, -44, 108, -20, -24, 9}};
    boost::array<double, 10> const d3a_cub =
        {{1000, -1800, -120, 2124, -1032, -684, 638, -18, -108, 27}};
    polynomial<T> const asqr(d3a_sqr.begin(), d3a_sqr.end());
    polynomial<T> const acub(d3a_cub.begin(), d3a_cub.end());

    BOOST_CHECK_EQUAL(pow(a, 0), one);
    BOOST_CHECK_EQUAL(pow(a, 1), a);
    BOOST_CHECK_EQUAL(pow(a, 2), asqr);
    BOOST_CHECK_EQUAL(pow(a, 3), acub);
    BOOST_CHECK_EQUAL(pow(a, 4), pow(asqr, 2));
    BOOST_CHECK_EQUAL(pow(a, 5), asqr * acub);
    BOOST_CHECK_EQUAL(pow(a, 6), pow(acub, 2));
    BOOST_CHECK_EQUAL(pow(a, 7), acub * acub * a);

    BOOST_CHECK_THROW(pow(a, -1), std::domain_error);
    BOOST_CHECK_EQUAL(pow(one, 137), one);
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_bool, T, all_test_types)
{
    polynomial<T> const zero;
    polynomial<T> const a(d0a.begin(), d0a.end());
    BOOST_CHECK_EQUAL(bool(zero), false);
    BOOST_CHECK_EQUAL(bool(a), true);
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_set_zero, T, all_test_types)
{
    polynomial<T> const zero;
    polynomial<T> a(d0a.begin(), d0a.end());
    a.set_zero();
    BOOST_CHECK_EQUAL(a, zero);
    a.set_zero(); // Ensure that setting zero to zero is a no-op.
    BOOST_CHECK_EQUAL(a, zero);
}
