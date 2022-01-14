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

#if !defined(TEST_MPF_50) && !defined(TEST_MPFR_50)
#define TEST_MPF_50
#define TEST_MPFR_50

#ifdef _MSC_VER
#pragma message("CAUTION!!: No backend type specified so testing everything.... this will take some time!!")
#endif
#ifdef __GNUC__
#pragma warning "CAUTION!!: No backend type specified so testing everything.... this will take some time!!"
#endif

#endif

#include <boost/multiprecision/mpfr.hpp>

#if defined(TEST_MPF_50)
#include <boost/multiprecision/gmp.hpp>
#endif

template <class T>
void thread_test_proc(unsigned digits)
{
   typedef boost::multiprecision::number<boost::multiprecision::mpfr_float_backend<1000> > mpfr_float_1000;

   if (!mpfr_buildopt_tls_p())
      return;

   T::thread_default_precision(digits);

   T result, value;

   value = boost::math::constants::pi<T>();
   result.assign(boost::math::constants::pi<mpfr_float_1000>().str());
   BOOST_CHECK_LE(boost::math::epsilon_difference(value, result), 20);
   BOOST_CHECK_EQUAL(value.precision(), digits);

   for (unsigned i = 20; i < 500; ++i)
   {
      T arg(i);
      arg /= 3;
      value = boost::math::tgamma(arg);
      BOOST_CHECK_EQUAL(value.precision(), digits);
      result.assign(boost::math::tgamma(mpfr_float_1000(arg)).str());
      BOOST_CHECK_LE(boost::math::epsilon_difference(value, result), 800);
   }

   //
   // Try tanh_sinh integration:
   //
   using namespace boost::math::constants;
   boost::math::quadrature::tanh_sinh<T> integrator;
   T                                     tol = 500;
   // Example 1:
   auto f1         = [](const T& t) { return static_cast<T>(t * boost::math::log1p(t)); };
   T Q          = integrator.integrate(f1, (T)0, (T)1);
   T Q_expected = half<T>() * half<T>();
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);

   // Example 2:
   auto f2    = [](const T& t) { return static_cast<T>(t * t * atan(t)); };
   Q          = integrator.integrate(f2, (T)0, (T)1);
   Q_expected = (pi<T>() - 2 + 2 * ln_two<T>()) / 12;
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);

   // Example 3:
   auto f3    = [](const T& t) { return static_cast<T>(exp(t) * cos(t)); };
   Q          = integrator.integrate(f3, (T)0, half_pi<T>());
   Q_expected = boost::math::expm1(half_pi<T>()) * half<T>();
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);

   // Example 4:
   auto f4    = [](T x) -> T { T t0 = sqrt(x*x + 2); return atan(t0)/(t0*(x*x+1)); };
   Q          = integrator.integrate(f4, (T)0, (T)1);
   Q_expected = 5 * pi<T>() * pi<T>() / 96;
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);

   // Example 5:
   auto f5    = [](const T& t)->T { return sqrt(t) * log(t); };
   Q          = integrator.integrate(f5, (T)0, (T)1);
   Q_expected = -4 / (T)9;
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);

   // Example 6:
   auto f6    = [](const T& t)->T { return sqrt(1 - t * t); };
   Q          = integrator.integrate(f6, (T)0, (T)1);
   Q_expected = pi<T>() / 4;
   BOOST_CHECK_EQUAL(Q.precision(), digits);
   BOOST_CHECK_EQUAL(Q_expected.precision(), digits);
   BOOST_CHECK_LE(boost::math::epsilon_difference(Q, Q_expected), tol);
}

int main()
{
#ifdef TEST_MPF_50
   {
      std::thread t1(thread_test_proc<boost::multiprecision::mpf_float>, 35);
      std::thread t2(thread_test_proc<boost::multiprecision::mpf_float>, 55);
      std::thread t3(thread_test_proc<boost::multiprecision::mpf_float>, 75);
      std::thread t4(thread_test_proc<boost::multiprecision::mpf_float>, 105);
      std::thread t5(thread_test_proc<boost::multiprecision::mpf_float>, 305);

      t1.join();
      t2.join();
      t3.join();
      t4.join();
      t5.join();
   }
#endif
#ifdef TEST_MPFR_50
   {
      std::thread t1(thread_test_proc<boost::multiprecision::mpfr_float>, 35);
      std::thread t2(thread_test_proc<boost::multiprecision::mpfr_float>, 55);
      std::thread t3(thread_test_proc<boost::multiprecision::mpfr_float>, 75);
      std::thread t4(thread_test_proc<boost::multiprecision::mpfr_float>, 105);
      std::thread t5(thread_test_proc<boost::multiprecision::mpfr_float>, 305);

      t1.join();
      t2.join();
      t3.join();
      t4.join();
      t5.join();
   }
#endif
   return boost::report_errors();
}
