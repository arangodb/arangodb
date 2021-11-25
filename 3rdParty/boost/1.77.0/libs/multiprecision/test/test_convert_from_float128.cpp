///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#ifdef HAS_FLOAT128

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
   //
   // Some basic sanity checks first:
   //
   BOOST_CHECK_EQUAL(std::numeric_limits<float128>::epsilon(), float128("1.92592994438723585305597794258492732e-34"));
   BOOST_CHECK_EQUAL((std::numeric_limits<float128>::min)(), float128("3.36210314311209350626267781732175260e-4932"));
   BOOST_CHECK_EQUAL((std::numeric_limits<float128>::max)(), float128("1.18973149535723176508575932662800702e4932"));
   BOOST_CHECK_EQUAL((std::numeric_limits<float128>::denorm_min)(), float128("6.475175119438025110924438958227646552e-4966"));
   BOOST_CHECK((boost::math::isinf)((std::numeric_limits<float128>::infinity)()));
   BOOST_CHECK((std::numeric_limits<float128>::infinity)() > 0);
   BOOST_CHECK((boost::math::isnan)((std::numeric_limits<float128>::quiet_NaN)()));

   test_convert<float128, cpp_int>();
   test_convert<float128, int128_t>();
   test_convert<float128, uint128_t>();

   test_convert<float128, cpp_rational>();

   test_convert<float128, cpp_dec_float_50>();

#if defined(HAS_GMP)
   test_convert<float128, mpz_int>();
   test_convert<float128, mpq_rational>();
   test_convert<float128, mpf_float_50>();
#endif
#if defined(HAS_MPFR)
   test_convert<float128, mpfr_float_50>();
#endif
#if defined(HAS_MPFI)
   test_convert<float128, mpfi_float_50>();
#endif
#ifdef HAS_TOMMATH
   test_convert<float128, tom_int>();
   test_convert<float128, tom_rational>();
#endif
   return boost::report_errors();
}

#else

int main() { return 0; }

#endif
