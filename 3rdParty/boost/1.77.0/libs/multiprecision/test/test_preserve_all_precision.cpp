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

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/gmp.hpp>
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
   else if constexpr (std::numeric_limits<Other>::is_integer)
   {
      return (std::numeric_limits<Other>::max)();
   }
   else
   {
      using std::ldexp;
      return ldexp(Other(1) / 3, 30);
   }
}

template <class T>
unsigned precision_of(const T&)
{
   return std::numeric_limits<T>::min_exponent ? std::numeric_limits<T>::digits10 : 1 + std::numeric_limits<T>::digits10;
}

template <class Backend, boost::multiprecision::expression_template_option ET>
typename std::enable_if<boost::multiprecision::detail::is_variable_precision<boost::multiprecision::number<Backend, ET> >::value, unsigned>::type 
   precision_of(const boost::multiprecision::number<Backend, ET>& val)
{
   return val.precision();
}
template <class Backend, boost::multiprecision::expression_template_option ET>
typename std::enable_if<!boost::multiprecision::detail::is_variable_precision<boost::multiprecision::number<Backend, ET> >::value &&
    std::numeric_limits<boost::multiprecision::number<Backend, ET>>::is_integer 
   && !std::numeric_limits<boost::multiprecision::number<Backend, ET>>::is_bounded, unsigned>::type 
   precision_of(const boost::multiprecision::number<Backend, ET>& val)
{
   return 1 + boost::multiprecision::detail::digits2_2_10(1 + msb(val) - lsb(val));
}
template <class Backend, boost::multiprecision::expression_template_option ET>
typename std::enable_if<!boost::multiprecision::detail::is_variable_precision<boost::multiprecision::number<Backend, ET> >::value &&
    !std::numeric_limits<boost::multiprecision::number<Backend, ET>>::is_integer 
   && std::numeric_limits<boost::multiprecision::number<Backend, ET>>::is_exact, unsigned>::type 
   precision_of(const boost::multiprecision::number<Backend, ET>& val)
{
   return (std::max)(precision_of(numerator(val)), precision_of(denominator(val)));
}

template <class T, class Other>
void test_mixed()
{
   T::thread_default_precision(10);
   T::thread_default_variable_precision_options(boost::multiprecision::variable_precision_options::preserve_all_precision);
   Other big_a(make_other_big_value<Other>()), big_b(make_other_big_value<Other>()), big_c(make_other_big_value<Other>()), big_d(make_other_big_value<Other>());

   unsigned target_precision;
   if constexpr (std::is_integral_v<Other>)
   {
      if constexpr (std::is_unsigned_v<Other>)
      {
         using backend_t = typename T::backend_type;
         using smallest_unsigned = std::tuple_element_t<0, typename backend_t::unsigned_types>;
         target_precision        = (std::max)(T::thread_default_precision(), (std::max)(precision_of(big_a), precision_of(smallest_unsigned(0))));
      }
      else
      {
         using backend_t = typename T::backend_type;
         using smallest_signed = std::tuple_element_t<0, typename backend_t::signed_types>;
         target_precision        = (std::max)(T::thread_default_precision(), (std::max)(precision_of(big_a), precision_of(smallest_signed(0))));
      }
   }
   else if constexpr (std::is_floating_point_v<Other>)
   {
      using backend_t         = typename T::backend_type;
      using smallest_float = std::tuple_element_t<0, typename backend_t::float_types>;
      target_precision        = (std::max)(T::thread_default_precision(), (std::max)(precision_of(big_a), precision_of(smallest_float(0))));
   }
   else
      target_precision = (std::max)(T::thread_default_precision(), precision_of(big_a));

   T a(big_a);
   // We don't guarentee equivalent decimal precision, only that we can round trip (same number of bits):
   BOOST_CHECK_LE(std::abs((int)a.precision() - (int)target_precision), 2);
   if constexpr(!std::numeric_limits<Other>::is_exact || std::numeric_limits<Other>::is_integer)
   {
      BOOST_CHECK_EQUAL(static_cast<Other>(a), big_a);
   }
   else
   {
      // Other is a rational - we can't round trip!
      T r = static_cast<T>(static_cast<Other>(a));
      if constexpr ((boost::multiprecision::number_category<T>::value == boost::multiprecision::number_kind_floating_point) && std::is_same_v<T, typename T::value_type>)
      {// doesn't work for intervals:
         BOOST_CHECK_EQUAL(r, a);
      }
   }
   T b(std::move(big_d));
   BOOST_CHECK_LE(std::abs((int)b.precision() - (int)target_precision), 2);
   if constexpr (std::is_assignable_v<T, Other>)
   {
      a = new_value<T>();
      BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
      a = big_b;
      BOOST_CHECK_LE(std::abs((int)a.precision() - (int)target_precision), 2);
      if constexpr (!std::numeric_limits<Other>::is_exact || std::numeric_limits<Other>::is_integer)
      {
         BOOST_CHECK_EQUAL(static_cast<Other>(a), big_b);
      }
      else
      {
         // Other is a rational - we can't round trip!
         T r = static_cast<T>(static_cast<Other>(a));
         BOOST_CHECK_EQUAL(r, a);
      }
      b = new_value<T>();
      BOOST_CHECK_EQUAL(b.precision(), T::thread_default_precision());
      b = std::move(big_c);
      BOOST_CHECK_LE(std::abs((int)b.precision() - (int)target_precision), 2);
      b = new_value<T>();
      BOOST_CHECK_EQUAL(b.precision(), T::thread_default_precision());

      if constexpr (!std::is_assignable_v<Other, T>)
      {
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a = b + big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a = b * big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a = b - big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a = b / big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a += big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a -= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a *= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
         a = new_value<T>();
         BOOST_CHECK_EQUAL(a.precision(), T::thread_default_precision());
         a /= big_a;
         BOOST_CHECK_EQUAL(a.precision(), target_precision);
      }
   }
   if constexpr (!std::is_same_v<T, typename T::value_type>)
   {
      T cc(big_a, big_b);
      BOOST_CHECK_LE(std::abs((int)cc.precision() - (int)target_precision), 2);
      T dd(big_a, big_b, 55);
      BOOST_CHECK_EQUAL(dd.precision(), 55);
      T aa = new_value<T>();
      BOOST_CHECK_EQUAL(aa.precision(), 10);
      aa.assign(big_a);
      BOOST_CHECK_LE(std::abs((int)aa.precision() - (int)target_precision), 2);
      aa.assign(big_a, big_b);
      BOOST_CHECK_LE(std::abs((int)aa.precision() - (int)target_precision), 2);
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
         BOOST_CHECK_EQUAL(aa.precision(), T::thread_default_precision());
         aa.assign(big_a);
         BOOST_CHECK_EQUAL(aa.precision(), target_precision);
      }
   }
}


template <class T>
void test()
{
   T::thread_default_precision(100);
   T::thread_default_variable_precision_options(boost::multiprecision::variable_precision_options::preserve_all_precision);

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
   // cpp_dec_float has guard digits which breaks our tests:
   //test_mixed<T, boost::multiprecision::cpp_dec_float_100>();
#if !defined(TEST_MPFR) && !defined(TEST_MPFI) && !defined(TEST_MPC)
   test_mixed<T, boost::multiprecision::mpf_float_100>();
#endif
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
