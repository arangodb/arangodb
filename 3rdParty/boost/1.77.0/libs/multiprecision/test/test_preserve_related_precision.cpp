///////////////////////////////////////////////////////////////
//  Copyright 2021 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <thread>
#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include <boost/math/special_functions/relative_difference.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <boost/math/quadrature/tanh_sinh.hpp>
#include "test.hpp"

#if !defined(TEST_MPF) && !defined(TEST_MPFR) && !defined(TEST_MPFI) && !defined(TEST_MPC)
#define TEST_MPF
#define TEST_MPFR
#define TEST_MPFI
#define TEST_MPC

#ifdef _MSC_VER
#pragma message("CAUTION!!: No backend type specified so testing everything.... this will take some time!!")
#endif
#ifdef __GNUC__
#pragma warning "CAUTION!!: No backend type specified so testing everything.... this will take some time!!"
#endif

#endif

#include <boost/multiprecision/gmp.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#if defined(TEST_MPFR)
#include <boost/multiprecision/mpfr.hpp>
#endif
#if defined(TEST_MPFI)
#include <boost/multiprecision/mpfi.hpp>
#endif
#if defined(TEST_MPC)
#include <boost/multiprecision/mpc.hpp>
#endif

template <class T>
T new_value()
{
   return T("0.1");
}

template <class Other>
Other make_other_big_value()
{
   if constexpr (std::numeric_limits<Other>::is_integer && !std::numeric_limits<Other>::is_bounded)
      return (Other(1) << 1000) + 1;
   else if constexpr (!std::numeric_limits<Other>::is_integer && std::numeric_limits<Other>::is_exact && (std::numeric_limits<Other>::max_exponent == std::numeric_limits<Other>::min_exponent))
   {
      using value_type = typename Other::value_type;
      return Other(1) / ((value_type(1) << 1000) + 1);
   }
   else
      return (std::numeric_limits<Other>::max)();
}

template <class T, class U>
struct is_related_type : public std::false_type {};

template <>
struct is_related_type<boost::multiprecision::mpf_float, boost::multiprecision::mpf_float_100> : public std::true_type
{};
template <>
struct is_related_type<boost::multiprecision::number<boost::multiprecision::gmp_float<0>, boost::multiprecision::et_off>, boost::multiprecision::mpf_float_100> : public std::true_type
{};

#if defined(TEST_MPFR)
template <>
struct is_related_type<boost::multiprecision::mpfr_float, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
template <>
struct is_related_type<boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<0>, boost::multiprecision::et_off>, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
#endif
#if defined(TEST_MPFI)
template <>
struct is_related_type<boost::multiprecision::mpfi_float, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
template <>
struct is_related_type<boost::multiprecision::number<boost::multiprecision::mpfi_float_backend<0>, boost::multiprecision::et_off>, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
#endif
#if defined(TEST_MPC)
template <>
struct is_related_type<boost::multiprecision::mpc_complex, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
template <>
struct is_related_type<boost::multiprecision::number<boost::multiprecision::mpc_complex_backend<0>, boost::multiprecision::et_off>, boost::multiprecision::mpfr_float_100> : public std::true_type
{};
#endif

template <class T, class Other>
void test_mixed()
{
   T::thread_default_precision(10);
   T::thread_default_variable_precision_options(boost::multiprecision::variable_precision_options::preserve_related_precision);
   Other big_a(make_other_big_value<Other>()), big_b(make_other_big_value<Other>()), big_c(make_other_big_value<Other>()), big_d(make_other_big_value<Other>());

   unsigned target_precision;
   if constexpr (is_related_type<T, Other>::value)
      target_precision = std::numeric_limits<Other>::digits10;
   else
      target_precision = T::thread_default_precision();

   T a(big_a);
   BOOST_CHECK_EQUAL(a.precision(), target_precision);
   T b(std::move(big_d));
   BOOST_CHECK_EQUAL(b.precision(), target_precision);
   if constexpr (std::is_assignable_v<T, Other>)
   {
      a = big_b;
      BOOST_CHECK_EQUAL(a.precision(), target_precision);
      b = std::move(big_c);
      BOOST_CHECK_EQUAL(b.precision(), target_precision);

      if constexpr (!std::is_assignable_v<Other, T>)
      {
         a = b + big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = b * big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = b - big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = b / big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a += big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a -= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a *= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a /= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
      }
   }
   if constexpr (!std::is_same_v<T, typename T::value_type>)
   {
      T cc(big_a, big_b);
      BOOST_CHECK_EQUAL(cc.precision(), target_precision);
      T dd(big_a, big_b, 55);
      BOOST_CHECK_EQUAL(dd.precision(), 55);
      T aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 10);
      aa.assign(big_a);
      BOOST_CHECK_EQUAL(aa.precision(), target_precision);
      aa.assign(big_a, big_b);
      BOOST_CHECK_EQUAL(aa.precision(), target_precision);
      if constexpr (0 && std::is_constructible_v<T, Other, Other, int>)
      {
         aa.assign(big_a, big_b, 55);
         BOOST_CHECK_EQUAL(aa.precision(), 55);
      }
   }
   else
   {
      if constexpr (std::is_constructible_v<T, Other, int>)
      {
         T aa(big_a, 55);
         BOOST_CHECK_EQUAL(aa.precision(), 55);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), T::thread_default_precision());
         aa.assign(big_a, 55);
         BOOST_CHECK_EQUAL(aa.precision(), 55);
      }
      else
      {
         T aa;
         BOOST_CHECK_EQUAL(aa.precision(), target_precision);
         aa.assign(big_a);
         BOOST_CHECK_EQUAL(aa.precision(), target_precision);
      }
   }
}


template <class T>
void test()
{
   T::thread_default_precision(100);
   T::thread_default_variable_precision_options(boost::multiprecision::variable_precision_options::preserve_related_precision);

   T hp1("0.1"), hp2("0.3"), hp3("0.11"), hp4("0.1231");

   BOOST_CHECK_EQUAL(hp1.precision(), 100);
   BOOST_CHECK_EQUAL(hp2.precision(), 100);

   T::thread_default_precision(35);

   T a(hp1);
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a = hp1;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a = std::move(hp1);
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   T b(std::move(hp2));
   BOOST_CHECK_EQUAL(b.precision(), 100);
   b = new_value<T>();
   BOOST_CHECK_EQUAL(b.precision(), 35);

   a = b + hp3;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a = hp3 * b;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a += hp3;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a *= hp4;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);
   a -= b * hp3;
   BOOST_CHECK_EQUAL(a.precision(), 100);
   a = new_value<T>();
   BOOST_CHECK_EQUAL(a.precision(), 35);

   if constexpr (!std::is_same_v<T, typename T::value_type>)
   {
      //
      // If we have a component type: ie we are an interval or a complex number, then
      // operations involving the component type should match those of T:
      //
      using component_t = typename T::value_type;
      component_t::thread_default_precision(100);
      component_t::thread_default_variable_precision_options(boost::multiprecision::variable_precision_options::preserve_source_precision);

      component_t cp1("0.1"), cp2("0.3"), cp3("0.11"), cp4("0.1231");

      BOOST_CHECK_EQUAL(cp1.precision(), 100);
      BOOST_CHECK_EQUAL(cp2.precision(), 100);

      component_t::thread_default_precision(35);

      T aa(cp1);
      BOOST_CHECK_EQUAL(aa.precision(), 100);
      T cc(cp1, cp2);
      BOOST_CHECK_EQUAL(cc.precision(), 100);
      T dd(cp1, cp2, 55);
      BOOST_CHECK_EQUAL(dd.precision(), 55);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
      if constexpr (std::is_assignable_v<T, component_t>)
      {
         aa = cp1;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
         aa = std::move(cp1);
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
      }
      T bb(std::move(cp2));
      BOOST_CHECK_EQUAL(bb.precision(), 100);
      bb = new_value<T>();
      BOOST_CHECK_EQUAL(bb.precision(), 35);

      if constexpr (boost::multiprecision::is_compatible_arithmetic_type<component_t, T>::value)
      {
         aa = bb + cp3;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
         aa = cp3 * bb;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
         aa += cp3;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         aa -= cp3;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
         aa *= cp4;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         aa /= cp4;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
         aa -= bb * cp3;
         BOOST_CHECK_EQUAL(aa.precision(), 100);
         aa = new_value<T>();
         BOOST_CHECK_EQUAL(aa.precision(), 35);
      }
      aa.assign(cp1);
      BOOST_CHECK_EQUAL(aa.precision(), 100);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
      aa.assign(cp1, cp2);
      BOOST_CHECK_EQUAL(aa.precision(), 100);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
      aa.assign(cp1, cp2, 20);
      BOOST_CHECK_EQUAL(aa.precision(), 20);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
   }
   else
   {
      T aa(hp4, 20);
      BOOST_CHECK_EQUAL(aa.precision(), 20);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
      aa.assign(hp4);
      BOOST_CHECK_EQUAL(aa.precision(), 100);
      aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 35);
      aa.assign(hp4, 20);
      BOOST_CHECK_EQUAL(aa.precision(), 20);
   }

   test_mixed<T, char>();
   test_mixed<T, unsigned char>();
   test_mixed<T, signed char>();
   test_mixed<T, short>();
   test_mixed<T, unsigned short>();
   test_mixed<T, int>();
   test_mixed<T, unsigned int>();
   test_mixed<T, long>();
   test_mixed<T, unsigned long>();
   test_mixed<T, long long>();
   test_mixed<T, unsigned long long>();
   test_mixed<T, float>();
   test_mixed<T, double>();
   test_mixed<T, long double>();
   //
   // Test with other compatible multiprecision types:
   //
   test_mixed<T, boost::multiprecision::mpz_int>();
   test_mixed<T, boost::multiprecision::cpp_int>();
   test_mixed<T, boost::multiprecision::mpq_rational>();
   test_mixed<T, boost::multiprecision::cpp_rational>();
   test_mixed<T, boost::multiprecision::cpp_bin_float_100>();
   test_mixed<T, boost::multiprecision::cpp_dec_float_100>();
   test_mixed<T, boost::multiprecision::mpf_float_100>();
#if defined(TEST_MPFR) || defined(TEST_MPC) || defined(TEST_MPFI)
   test_mixed<T, boost::multiprecision::mpfr_float_100>();
#endif
}

int main()
{
#ifdef TEST_MPF
   test<boost::multiprecision::mpf_float>();
   test<boost::multiprecision::number<boost::multiprecision::gmp_float<0>, boost::multiprecision::et_off>>();
#endif
#ifdef TEST_MPFR
   test<boost::multiprecision::mpfr_float>();
   test<boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<0>, boost::multiprecision::et_off> >();
#endif
#ifdef TEST_MPFI
   test<boost::multiprecision::mpfi_float>();
   test<boost::multiprecision::number<boost::multiprecision::mpfi_float_backend<0>, boost::multiprecision::et_off> >();
#endif
#ifdef TEST_MPC
   test<boost::multiprecision::mpc_complex>();
   test<boost::multiprecision::number<boost::multiprecision::mpc_complex_backend<0>, boost::multiprecision::et_off> >();
#endif
   return boost::report_errors();
}
