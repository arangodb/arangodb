//  (C) Copyright Jeremy Murphy 2015.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>
#define BOOST_TEST_MAIN
#include <boost/array.hpp>
#include <boost/math/common_factor_rt.hpp>
#include <boost/mpl/list.hpp>
#include <boost/test/test_case_template.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <utility>
#include <boost/random.hpp>

//
// Naive implmentation, any fancy versions we create should always agree with this:
//
template <class T>
typename boost::enable_if_c<std::numeric_limits<T>::is_signed, T>::type unsigned_abs(T v) { return v < 0 ? -v : v; }
template <class T>
typename boost::disable_if_c<std::numeric_limits<T>::is_signed, T>::type unsigned_abs(T v) { return v; }

template <class T>
T euclid_textbook(T a, T b)
{
   using std::swap;
   if(a < b)
      swap(a, b);
   while(b)
   {
      T t = b;
      b = a % b;
      a = t;
   }
   return unsigned_abs(a);
}

typedef boost::mpl::list<boost::int32_t
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
   , boost::int64_t, boost::multiprecision::cpp_int
#endif
> signed_integral_test_types;

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

typedef boost::mpl::list<boost::uint32_t
#if !BOOST_WORKAROUND(BOOST_MSVC, <= 1500)
   , boost::uint64_t, boost::multiprecision::uint256_t
#endif
> unsigned_integral_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(test_unsigned, T, unsigned_integral_test_types)
{
    T a = boost::math::gcd(static_cast<T>(40902), static_cast<T>(24140));
    BOOST_CHECK_EQUAL(a, static_cast<T>(34));
    a = boost::math::gcd(static_cast<T>(1836311903), static_cast<T>(2971215073)); // 46th and 47th Fibonacci numbers. 47th is prime.
    BOOST_CHECK_EQUAL(a, static_cast<T>(1));
}

typedef boost::mpl::list<boost::int32_t, boost::int64_t> short_signed_integral_test_types;
typedef boost::mpl::list<boost::uint32_t, boost::uint64_t> short_unsigned_integral_test_types;

BOOST_AUTO_TEST_CASE_TEMPLATE(signed_random_test, T, short_signed_integral_test_types)
{
   boost::random::mt19937 gen;
   boost::uniform_int<T> dist((std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());
   for(unsigned i = 0; i < 100000; ++i)
   {
      T u = dist(gen);
      T v = dist(gen);
      BOOST_CHECK_EQUAL(boost::math::gcd(u, v), euclid_textbook(u, v));
   }
}

BOOST_AUTO_TEST_CASE_TEMPLATE(test_random_unsigned, T, short_unsigned_integral_test_types)
{
   boost::random::mt19937 gen;
   boost::uniform_int<T> dist((std::numeric_limits<T>::min)(), (std::numeric_limits<T>::max)());
   for(unsigned i = 0; i < 100000; ++i)
   {
      T u = dist(gen);
      T v = dist(gen);
      BOOST_CHECK_EQUAL(boost::math::gcd(u, v), euclid_textbook(u, v));
   }
}


BOOST_AUTO_TEST_CASE_TEMPLATE(test_gcd_range, T, unsigned_integral_test_types)
{
    std::vector<T> a;
    typedef typename std::vector<T>::iterator I;
    std::pair<T, I> d;
    a.push_back(40902);
    d = boost::math::gcd_range(a.begin(), a.end());
    BOOST_CHECK(d == std::make_pair(T(40902), a.end()));
    a.push_back(24140);
    d = boost::math::gcd_range(a.begin(), a.end());
    BOOST_CHECK(d == std::make_pair(T(34), a.end()));
    a.push_back(85);
    d = boost::math::gcd_range(a.begin(), a.end());
    BOOST_CHECK(d == std::make_pair(T(17), a.end()));
    a.push_back(23893);
    d = boost::math::gcd_range(a.begin(), a.end());
    BOOST_CHECK(d == std::make_pair(T(1), a.end()));
    a.push_back(1024);
    d = boost::math::gcd_range(a.begin(), a.end());
    BOOST_CHECK(d == std::make_pair(T(1), a.end() - 1));
}
