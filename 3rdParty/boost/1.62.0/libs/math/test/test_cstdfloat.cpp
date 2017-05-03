//  Copyright Christopher Kormanyos 2014.
//  Copyright John Maddock 2014.
//  Copyright Paul A. Bristow 2014.

//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <cmath>
#include <complex>
#include <limits>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <boost/math/cstdfloat/cstdfloat_types.hpp>

#ifdef _MSC_VER
#  pragma warning(disable : 4127) // conditional expression is constant.
#  pragma warning(disable : 4512) // assignment operator could not be generated.
#  pragma warning(disable : 4996) // use -D_SCL_SECURE_NO_WARNINGS.
#endif

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp> // Boost.Test
#include <boost/test/floating_point_comparison.hpp>

//
// DESCRIPTION:
// ~~~~~~~~~~~~
//
// This file tests the implementation of floating-point typedefs having
// specified widths, as implemented in <boost/cstdfloat.hpp> and described
// in N3626 (proposed for C++14).

// For more information on <boost/cstdfloat.hpp> and the corresponding
// proposal of "Floating-Point Typedefs Having Specified Widths",
// see: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2013/n3626.pdf

// The tests:
//
// Perform sanity checks on boost::float16_t, boost::float32_t,
// boost::float64_t, boost::float80_t, and boost::float128_t when
// these types are available. In the sanity checks, we verify the
// formal behavior of the types and the macros for creating literal
// floating-point constants.
//
// An extended check is included for boost::float128_t. This checks
// the functionality of <cmath>, I/O stream operations, and <complex>
// functions for boost::float128_t.

#define TEST_CSTDFLOAT_SANITY_CHECK(the_digits)                                                      \
void sanity_check_##the_digits##_func()                                                              \
{                                                                                                    \
  typedef boost::float##the_digits##_t float_type;                                                   \
                                                                                                     \
  BOOST_CONSTEXPR_OR_CONST int my_digits10 = std::numeric_limits<float_type>::digits10;              \
                                                                                                     \
  {                                                                                                  \
    BOOST_CONSTEXPR_OR_CONST float_type x =                                                          \
      BOOST_FLOAT##the_digits##_C(0.33333333333333333333333333333333333333333);                      \
    std::stringstream ss;                                                                            \
    ss << std::setprecision(my_digits10 - 1)                                                         \
       << x;                                                                                         \
    std::string str = "0.";                                                                          \
    str += std::string(std::string::size_type(my_digits10 - 1), char('3'));                          \
    BOOST_CHECK_EQUAL( ss.str(), str );                                                              \
  }                                                                                                  \
  {                                                                                                  \
    BOOST_CONSTEXPR_OR_CONST float_type x =                                                          \
      BOOST_FLOAT##the_digits##_C(0.66666666666666666666666666666666666666666);                      \
    std::stringstream ss;                                                                            \
    ss << std::setprecision(my_digits10 - 1)                                                         \
       << x;                                                                                         \
    std::string str = "0.";                                                                          \
    str += std::string(std::string::size_type(my_digits10 - 2), char('6'));                          \
    str += "7";                                                                                      \
    BOOST_CHECK_EQUAL( ss.str(), str );                                                              \
  }                                                                                                  \
  {                                                                                                  \
    const float_type x = BOOST_FLOAT##the_digits##_C(1.0) / test_cstdfloat::zero;                    \
    const bool the_inf_test = (   std::numeric_limits<float_type>::has_infinity                      \
                               && (x == std::numeric_limits<float_type>::infinity()));               \
    BOOST_CHECK_EQUAL( the_inf_test, true );                                                         \
  }                                                                                                  \
  {                                                                                                  \
    using std::sqrt;                                                                                 \
    const float_type x = sqrt(float_type(test_cstdfloat::minus_one));                                \
    const bool the_nan_test = (   std::numeric_limits<float_type>::has_quiet_NaN                     \
                               && (x != x));                                                         \
    BOOST_CHECK_EQUAL( the_nan_test, true );                                                         \
  }                                                                                                  \
  {                                                                                                  \
    const bool the_lim_test =                                                                        \
      (std::numeric_limits<boost::floatmax_t>::digits >= std::numeric_limits<float_type>::digits);   \
    BOOST_CHECK_EQUAL( the_lim_test, true );                                                         \
  }                                                                                                  \
}

namespace test_cstdfloat
{
  int zero;
  int minus_one;

  #if defined(BOOST_FLOATMAX_C)
  BOOST_CONSTEXPR_OR_CONST int has_floatmax_t = 1;
  #else
  BOOST_CONSTEXPR_OR_CONST int has_floatmax_t = 0;
  #endif

  #if defined(BOOST_FLOAT16_C)
  TEST_CSTDFLOAT_SANITY_CHECK(16)
  #endif

  #if defined(BOOST_FLOAT32_C)
  TEST_CSTDFLOAT_SANITY_CHECK(32)
  #endif

  #if defined(BOOST_FLOAT64_C)
  TEST_CSTDFLOAT_SANITY_CHECK(64)
  #endif

  #if defined(BOOST_FLOAT80_C)
  TEST_CSTDFLOAT_SANITY_CHECK(80)
  #endif

  #if defined(BOOST_FLOAT128_C)
  TEST_CSTDFLOAT_SANITY_CHECK(128)

  void extend_check_128_func()
  {
  }
  #endif // defined (BOOST_FLOAT128_C)
}

BOOST_AUTO_TEST_CASE(test_main)
{
  test_cstdfloat::zero      =  0;
  test_cstdfloat::minus_one = -1;

  // Perform basic sanity checks that verify both the existence of the proper
  // floating-point literal macros as well as the correct digit handling
  // for a given floating-point typedef having specified width.

  BOOST_CHECK_EQUAL( test_cstdfloat::has_floatmax_t, 1 );

  #if defined(BOOST_FLOAT16_C)
  test_cstdfloat::sanity_check_16_func();
  #endif

  #if defined(BOOST_FLOAT32_C)
  test_cstdfloat::sanity_check_32_func();
  #endif

  #if defined(BOOST_FLOAT64_C)
  test_cstdfloat::sanity_check_64_func();
  #endif

  #if defined(BOOST_FLOAT80_C)
  test_cstdfloat::sanity_check_80_func();
  #endif

  #if defined(BOOST_FLOAT128_C)
  test_cstdfloat::sanity_check_128_func();

  // Perform an extended check of boost::float128_t including
  // a variety of functions from the C++ standard library.
  test_cstdfloat::extend_check_128_func();
  #endif // defined (BOOST_FLOAT128_C)
}
