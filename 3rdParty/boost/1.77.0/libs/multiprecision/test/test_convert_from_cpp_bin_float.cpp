///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include "test.hpp"

#if defined(HAS_GMP)
#include <boost/multiprecision/gmp.hpp>
#endif
#if defined(HAS_MPFR)
#include <boost/multiprecision/mpfr.hpp>
#endif
#if defined(HAS_MPFI)
#include <boost/multiprecision/mpfi.hpp>
#endif
#ifdef HAS_TOMMATH
#include <boost/multiprecision/tommath.hpp>
#endif
#ifdef HAS_FLOAT128
#include <boost/multiprecision/float128.hpp>
#endif
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

using namespace boost::multiprecision;

#ifdef BOOST_MSVC
#pragma warning(disable : 4127)
#endif

template <class T>
T generate_random()
{
   //
   // Our aim is to fill up the mantissa of T with random bits,
   // the main loop simply multiplies by the largest random value
   // we produce and then adds the next random value.  It stops
   // when the next random value is too small to change the result,
   // and so "val" and "prev_val" end up with the same value.
   //
   // We start with an arbitrary value in "prev_val" - anything you
   // like as long as it's a value that our generator can not produce.
   //
   // At the end we ditch the current exponent, and replace with our own
   // randomly generated one.
   //
   typedef int                   e_type;
   static boost::random::mt19937 gen;
   T                             val      = gen();
   T                             prev_val = -1;
   while (val != prev_val)
   {
      val *= (gen.max)();
      prev_val = val;
      val += gen();
   }
   e_type e;
   val = frexp(val, &e);

   static boost::random::uniform_int_distribution<e_type> ui(-20, 20);
   return ldexp(val, ui(gen));
}

template <class From, class To>
void test_convert_neg_int(From from, const std::integral_constant<bool, true>&)
{
   from = -from;
   To t3(from);
   To t4 = from.template convert_to<To>();
   BOOST_CHECK_EQUAL(From(trunc(from)), From(t3));
   BOOST_CHECK_EQUAL(From(trunc(from)), From(t4));
}
template <class From, class To>
void test_convert_neg_int(From const&, const std::integral_constant<bool, false>&)
{
}

template <class From, class To>
void test_convert_imp(std::integral_constant<int, number_kind_floating_point> const&, std::integral_constant<int, number_kind_integer> const&)
{
   for (unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>();
      To   t1(from);
      To   t2 = from.template convert_to<To>();
      BOOST_CHECK_EQUAL(From(trunc(from)), From(t1));
      BOOST_CHECK_EQUAL(From(trunc(from)), From(t2));
      test_convert_neg_int<From, To>(from, std::integral_constant<bool, std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed > ());
   }
}

template <class From, class To>
void test_convert_neg_rat(From from, const std::integral_constant<bool, true>&)
{
   from = -from;
   To t3(from);
   To t4 = from.template convert_to<To>();
   BOOST_CHECK_EQUAL(From(t3), from);
   BOOST_CHECK_EQUAL(From(t4), from);
}
template <class From, class To>
void test_convert_rat_int(From const&, const std::integral_constant<bool, false>&)
{
}

template <class From, class To>
void test_convert_imp(std::integral_constant<int, number_kind_floating_point> const&, std::integral_constant<int, number_kind_rational> const&)
{
   for (unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>();
      To   t1(from);
      To   t2 = from.template convert_to<To>();
      BOOST_CHECK_EQUAL(From(t1), from);
      BOOST_CHECK_EQUAL(From(t2), from);
      test_convert_neg_rat<From, To>(from, std::integral_constant<bool, std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed > ());
   }
}

template <class From, class To>
void test_convert_neg_float(From from, const std::integral_constant<bool, true>&)
{
   from = -from;
   To t3(from);
   To t4 = from.template convert_to<To>();
   To answer(from.str());
   To tol = (std::max)(std::numeric_limits<To>::epsilon(), To(std::numeric_limits<From>::epsilon())) * 2;
   BOOST_CHECK_CLOSE_FRACTION(t3, answer, tol);
   BOOST_CHECK_CLOSE_FRACTION(t4, answer, tol);
}
template <class From, class To>
void test_convert_neg_float(From const&, const std::integral_constant<bool, false>&)
{
}

template <class From, class To>
void test_convert_imp(std::integral_constant<int, number_kind_floating_point> const&, std::integral_constant<int, number_kind_floating_point> const&)
{
   for (unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>();
      To   t1(from);
      To   t2 = from.template convert_to<To>();
      To   answer(from.str());
      To   tol = (std::max)(std::numeric_limits<To>::epsilon(), To(std::numeric_limits<From>::epsilon())) * 2;
      BOOST_CHECK_CLOSE_FRACTION(t1, answer, tol);
      BOOST_CHECK_CLOSE_FRACTION(t2, answer, tol);
      test_convert_neg_float<From, To>(from, std::integral_constant<bool, std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed > ());
   }
}

template <class From, class To>
void test_convert()
{
   test_convert_imp<From, To>(typename number_category<From>::type(), typename number_category<To>::type());
}

int main()
{
   test_convert<cpp_bin_float_50, cpp_int>();
   test_convert<cpp_bin_float_50, int128_t>();
   test_convert<cpp_bin_float_50, uint128_t>();
   test_convert<cpp_bin_float_quad, checked_int1024_t>();
   test_convert<cpp_bin_float_quad, checked_uint1024_t>();

   test_convert<cpp_bin_float_50, cpp_rational>();

   test_convert<cpp_bin_float_50, cpp_dec_float_50>();

#if defined(HAS_GMP)
   test_convert<cpp_bin_float_50, mpz_int>();
   test_convert<cpp_bin_float_50, mpq_rational>();
   test_convert<cpp_bin_float_50, mpf_float_50>();
#endif
#if defined(HAS_MPFR)
   test_convert<cpp_bin_float_50, mpfr_float_50>();
#endif
#if defined(HAS_MPFI)
   test_convert<cpp_bin_float_50, mpfi_float_50>();
#endif
#ifdef HAS_TOMMATH
   test_convert<cpp_bin_float_50, tom_int>();
   test_convert<cpp_bin_float_50, tom_rational>();
#endif
#ifdef HAS_FLOAT128
   test_convert<cpp_bin_float_50, float128>();
#endif
   return boost::report_errors();
}
