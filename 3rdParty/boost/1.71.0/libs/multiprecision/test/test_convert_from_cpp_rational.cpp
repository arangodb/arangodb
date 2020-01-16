///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random/mersenne_twister.hpp>
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
#pragma warning(disable:4127)
#endif


template <class T>
T generate_random_int(unsigned bits_wanted)
{
   static boost::random::mt19937 gen;
   typedef boost::random::mt19937::result_type random_type;

   T max_val;
   unsigned digits;
   if(std::numeric_limits<T>::is_bounded && (bits_wanted == (unsigned)std::numeric_limits<T>::digits))
   {
      max_val = (std::numeric_limits<T>::max)();
      digits = std::numeric_limits<T>::digits;
   }
   else
   {
      max_val = T(1) << bits_wanted;
      digits = bits_wanted;
   }

   unsigned bits_per_r_val = std::numeric_limits<random_type>::digits - 1;
   while((random_type(1) << bits_per_r_val) > (gen.max)()) --bits_per_r_val;

   unsigned terms_needed = digits / bits_per_r_val + 1;

   T val = 0;
   for(unsigned i = 0; i < terms_needed; ++i)
   {
      val *= (gen.max)();
      val += gen();
   }
   val %= max_val;
   return val;
}

template <class T>
T generate_random(unsigned bits_wanted)
{
   typedef typename component_type<T>::type int_type;
   T val(generate_random_int<int_type>(bits_wanted), generate_random_int<int_type>(bits_wanted));
   return val;
}

template <class From, class To>
void test_convert_neg_val(From from, const boost::mpl::true_&)
{
   from = -from;
   typename component_type<From>::type answer = numerator(from) / denominator(from);
   To t3(from);
   To t4 = from.template convert_to<To>();
   BOOST_CHECK_EQUAL(answer.str(), t3.str());
   BOOST_CHECK_EQUAL(answer.str(), t4.str());
}
template <class From, class To>
void test_convert_neg_val(From const&, const boost::mpl::false_&)
{
}

template <class From, class To>
void test_convert_imp(boost::mpl::int_<number_kind_rational> const&, boost::mpl::int_<number_kind_integer> const&)
{
   int bits_wanted = (std::min)((std::min)(std::numeric_limits<From>::digits, std::numeric_limits<To>::digits), 2000);

   for(unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>(bits_wanted);
      typename component_type<From>::type answer = numerator(from) / denominator(from);
      To t1(from);
      To t2 = from.template convert_to<To>();
      BOOST_CHECK_EQUAL(answer.str(), t1.str());
      BOOST_CHECK_EQUAL(answer.str(), t2.str());
      test_convert_neg_val<From, To>(from, boost::mpl::bool_<std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed>());
   }
}

template <class From, class To>
void test_convert_neg_float_val(From from, To const& tol, const boost::mpl::true_&)
{
   from = -from;
   To answer = To(numerator(from)) / To(denominator(from));
   To t3(from);
   To t4 = from.template convert_to<To>();
   BOOST_CHECK_CLOSE_FRACTION(answer, t3, tol);
   BOOST_CHECK_CLOSE_FRACTION(answer, t4, tol);
}
template <class From, class To>
void test_convert_neg_float_val(From const&, To const&, const boost::mpl::false_&)
{
}

template <class From, class To>
void test_convert_imp(boost::mpl::int_<number_kind_rational> const&, boost::mpl::int_<number_kind_floating_point> const&)
{
   int bits_wanted = (std::min)((std::min)(std::numeric_limits<From>::digits, std::numeric_limits<To>::digits), 2000);

   for(unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>(bits_wanted);
      To answer = To(numerator(from)) / To(denominator(from));
      To t1(from);
      To t2 = from.template convert_to<To>();
      To tol = std::numeric_limits<To>::is_specialized ? std::numeric_limits<To>::epsilon() : ldexp(To(1), 1 - bits_wanted);
      tol *= 2;
      BOOST_CHECK_CLOSE_FRACTION(answer, t1, tol);
      BOOST_CHECK_CLOSE_FRACTION(answer, t2, tol);
      test_convert_neg_float_val<From, To>(from, tol, boost::mpl::bool_<std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed>());
   }
}

template <class From, class To>
void test_convert_neg_rat_val(From from, const boost::mpl::true_&)
{
   from = -from;
   To t3(from);
   To t4 = from.template convert_to<To>();
   BOOST_CHECK_EQUAL(from.str(), t3.str());
   BOOST_CHECK_EQUAL(from.str(), t4.str());
}
template <class From, class To>
void test_convert_neg_rat_val(From const&, const boost::mpl::false_&)
{
}

template <class From, class To>
void test_convert_imp(boost::mpl::int_<number_kind_rational> const&, boost::mpl::int_<number_kind_rational> const&)
{
   int bits_wanted = (std::min)((std::min)(std::numeric_limits<From>::digits, std::numeric_limits<To>::digits), 2000);

   for(unsigned i = 0; i < 100; ++i)
   {
      From from = generate_random<From>(bits_wanted);
      To t1(from);
      To t2 = from.template convert_to<To>();
      BOOST_CHECK_EQUAL(from.str(), t1.str());
      BOOST_CHECK_EQUAL(from.str(), t2.str());
      test_convert_neg_rat_val<From, To>(from, boost::mpl::bool_<std::numeric_limits<From>::is_signed && std::numeric_limits<To>::is_signed>());
   }
}




template <class From, class To>
void test_convert()
{
   test_convert_imp<From, To>(typename number_category<From>::type(), typename number_category<To>::type());
}


int main()
{
   test_convert<cpp_rational, cpp_int>();
   test_convert<cpp_rational, int128_t>();
   test_convert<cpp_rational, uint128_t>();

   test_convert<cpp_rational, cpp_bin_float_50>();

   test_convert<cpp_rational, cpp_dec_float_50>();

#if defined(HAS_GMP)
   test_convert<cpp_rational, mpz_int>();
   test_convert<cpp_rational, mpq_rational>();
   test_convert<cpp_rational, mpf_float_50>();
#endif
#if defined(HAS_MPFR)
   test_convert<cpp_rational, mpfr_float_50>();
#endif
#if defined(HAS_MPFI)
   test_convert<cpp_rational, mpfi_float_50>();
#endif
#ifdef HAS_TOMMATH
   test_convert<cpp_rational, tom_int>();
   test_convert<cpp_rational, tom_rational>();
#endif
   return boost::report_errors();
}

