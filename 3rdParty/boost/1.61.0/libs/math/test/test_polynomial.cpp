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


#ifndef BOOST_NO_CXX11_HDR_INITIALIZER_LIST
BOOST_AUTO_TEST_CASE( test_initializer_list_construction )
{
    polynomial<double> a(begin(d3a), end(d3a));
    polynomial<double> b = {10, -6, -4, 3};
    polynomial<double> c{{10, -6, -4, 3}};
    BOOST_CHECK_EQUAL(a, b);
    BOOST_CHECK_EQUAL(b, c);
}
#endif


BOOST_AUTO_TEST_CASE( test_degree )
{
    polynomial<double> const zero = zero_element(std::multiplies< polynomial<double> >());
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
    polynomial<double> const zero = zero_element(std::multiplies< polynomial<double> >());
    polynomial<double> const one = identity_element(std::multiplies< polynomial<double> >());

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
    polynomial<int> const zero = zero_element(std::multiplies< polynomial<int> >());
    polynomial<int> const one = identity_element(std::multiplies< polynomial<int> >());
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
typedef boost::mpl::list<int, long, boost::multiprecision::cpp_int> integral_test_types;
typedef boost::mpl::list<double, boost::multiprecision::cpp_rational, boost::multiprecision::cpp_bin_float_single, boost::multiprecision::cpp_dec_float_50> non_integral_test_types;
typedef boost::mpl::joint_view<integral_test_types, non_integral_test_types> all_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE( test_addition, T, all_test_types )
{
    polynomial<T> const a(d3a.begin(), d3a.end());
    polynomial<T> const b(d1a.begin(), d1a.end());
    polynomial<T> const zero = zero_element(multiplies< polynomial<T> >());
    
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
    polynomial<T> const zero = zero_element(multiplies< polynomial<T> >());

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
    polynomial<T> const zero = zero_element(multiplies< polynomial<T> >());
    
    BOOST_CHECK_EQUAL(a * T(0), zero);
    BOOST_CHECK_EQUAL(a * zero, zero);
    BOOST_CHECK_EQUAL(zero * T(0), zero);
    BOOST_CHECK_EQUAL(zero * zero, zero);
    BOOST_CHECK_EQUAL(a * b, b * a);
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

