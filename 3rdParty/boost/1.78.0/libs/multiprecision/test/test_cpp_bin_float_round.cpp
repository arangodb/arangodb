// Copyright John Maddock 2016.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//
// This file takes way too long to run to be part of the main regression test suite,
// but is useful in probing for errors in cpp_bin_float's rounding code.
// It cycles through every single value of type float, and rounds those numbers
// plus some closely related ones and compares the results to those produced by MPFR.
//
#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/mpfr.hpp>
#include <boost/math/special_functions/next.hpp>
#include "test.hpp"

using namespace boost::multiprecision;

typedef number<mpfr_float_backend<35> >                                                     good_type;
typedef number<cpp_bin_float<std::numeric_limits<good_type>::digits, digit_base_2>, et_off> test_type;

int main()
{
   float f = (std::numeric_limits<float>::max)();

   do
   {
      float     fr1, fr2;
      good_type gf(f), gf2(f);
      test_type tf(f), tf2(f);
      fr1 = gf.convert_to<float>();
      fr2 = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);
      // next represenation:
      gf = boost::math::float_next(gf2);
      tf = boost::math::float_next(tf2);
      BOOST_CHECK_NE(gf, gf2);
      BOOST_CHECK_NE(tf, tf2);
      fr1 = gf.convert_to<float>();
      fr2 = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);
      // previous representation:
      gf = boost::math::float_prior(gf2);
      tf = boost::math::float_prior(tf2);
      BOOST_CHECK_NE(gf, gf2);
      BOOST_CHECK_NE(tf, tf2);
      fr1 = gf.convert_to<float>();
      fr2 = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);

      // Create ands test ties:
      int e;
      std::frexp(f, &e);
      float extra = std::ldexp(1.0f, e - std::numeric_limits<float>::digits - 1);
      gf          = gf2 += extra;
      tf          = tf2 += extra;
      fr1         = gf.convert_to<float>();
      fr2         = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);
      // next represenation:
      gf = boost::math::float_next(gf2);
      tf = boost::math::float_next(tf2);
      BOOST_CHECK_NE(gf, gf2);
      BOOST_CHECK_NE(tf, tf2);
      fr1 = gf.convert_to<float>();
      fr2 = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);
      // previous representation:
      gf = boost::math::float_prior(gf2);
      tf = boost::math::float_prior(tf2);
      BOOST_CHECK_NE(gf, gf2);
      BOOST_CHECK_NE(tf, tf2);
      fr1 = gf.convert_to<float>();
      fr2 = tf.convert_to<float>();
      BOOST_CHECK_EQUAL(fr1, fr2);

      f = boost::math::float_prior(f);
   } while (f);

   return boost::report_errors();
}
