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


typedef boost::mpl::list<int, long, boost::multiprecision::cpp_int> signed_integral_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_zero, T, signed_integral_test_types)
{
    T a = boost::math::gcd(static_cast<T>(2), static_cast<T>(0));
    BOOST_CHECK_EQUAL(a, static_cast<T>(2));
    a = boost::math::gcd(static_cast<T>(0), static_cast<T>(2));
    BOOST_CHECK_EQUAL(a, static_cast<T>(2));
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_signed, T, signed_integral_test_types)
{
    T a = boost::math::gcd(static_cast<T>(-40902), static_cast<T>(-24140));
    BOOST_CHECK_EQUAL(a, static_cast<T>(34));
    a = boost::math::gcd(static_cast<T>(40902), static_cast<T>(24140));
    BOOST_CHECK_EQUAL(a, static_cast<T>(34));
}

typedef boost::mpl::list<unsigned, unsigned long> unsigned_integral_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_unsigned, T, unsigned_integral_test_types)
{
    T a = boost::math::gcd(static_cast<T>(40902), static_cast<T>(24140));
    BOOST_CHECK_EQUAL(a, static_cast<T>(34));
    a = boost::math::gcd(static_cast<T>(1836311903), static_cast<T>(2971215073)); // 46th and 47th Fibonacci numbers. 47th is prime.
    BOOST_CHECK_EQUAL(a, static_cast<T>(1));
}
