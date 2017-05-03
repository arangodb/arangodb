// Copyright John Maddock 2013.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#if defined(TEST1) || defined(TEST2) || defined(TEST3) || defined(TEST4)
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#else
#include <boost/multiprecision/mpfr.hpp>
#endif
#include <boost/math/special_functions/next.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/chrono.hpp>
#include "test.hpp"
#include <boost/array.hpp>
#include <iostream>
#include <iomanip>

#ifdef BOOST_MSVC
#pragma warning(disable:4127)
#endif

template <class Clock>
struct stopwatch
{
   typedef typename Clock::duration duration;
   stopwatch()
   {
      m_start = Clock::now();
   }
   duration elapsed()
   {
      return Clock::now() - m_start;
   }
   void reset()
   {
      m_start = Clock::now();
   }

private:
   typename Clock::time_point m_start;
};

template <class T>
struct exponent_type 
{
   typedef int type;
};
template <class T, boost::multiprecision::expression_template_option ET>
struct exponent_type<boost::multiprecision::number<T, ET> >
{
   typedef typename T::exponent_type type;
};

template <class T>
T generate_random_float()
{
   BOOST_MATH_STD_USING
   typedef typename exponent_type<T>::type e_type;
   static boost::random::mt19937 gen;
   T val = gen();
   T prev_val = -1;
   while(val != prev_val)
   {
      val *= (gen.max)();
      prev_val = val;
      val += gen();
   }
   e_type e;
   val = frexp(val, &e);

   static const int max_exponent_value = (std::min)(static_cast<int>(std::numeric_limits<T>::max_exponent - std::numeric_limits<T>::digits - 20), 2000);
   static boost::random::uniform_int_distribution<e_type> ui(0, max_exponent_value);
   return ldexp(val, ui(gen));
}

template <class Float, class Rat>
void do_round_trip(const Float& val)
{
#ifndef BOOST_MP_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
   BOOST_MATH_STD_USING
   Rat rat(val);
   Float new_f(rat);
   BOOST_CHECK_EQUAL(val, new_f);
   //
   // Try adding or subtracting an insignificant amount
   // (0.25ulp) from rat and check that it rounds to the same value:
   //
   typename exponent_type<Float>::type e;
   Float t = frexp(val, &e);
   (void)t; // warning suppression
   e -= std::numeric_limits<Float>::digits + 2;
   BOOST_ASSERT(val == (val + ldexp(Float(1), e)));
   Rat delta, rounded;
   typedef typename boost::multiprecision::component_type<Rat>::type i_type;
   i_type i(1);
   i <<= (e < 0 ? -e : e);
   if(e > 0)
      delta.assign(i);
   else
      delta = Rat(i_type(1), i);
   rounded = rat + delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);
   rounded = rat - delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);

   delta /= 2;
   rounded = rat + delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);
   rounded = rat - delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);

   delta /= 2;
   rounded = rat + delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);
   rounded = rat - delta;
   new_f = static_cast<Float>(rounded);
   BOOST_CHECK_EQUAL(val, new_f);
#endif
}

template <class Float, class Rat>
void test_round_trip()
{
#ifndef BOOST_MP_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
   std::cout << "Testing types " << typeid(Float).name() << " <<==>> " << typeid(Rat).name() << std::endl;
   std::cout << "digits = " << std::numeric_limits<Float>::digits << std::endl;
   std::cout << "digits10 = " << std::numeric_limits<Float>::digits10 << std::endl;
#ifndef BOOST_NO_CXX11_NUMERIC_LIMITS
   std::cout << "max_digits10 = " << std::numeric_limits<Float>::max_digits10 << std::endl;
#endif

   stopwatch<boost::chrono::high_resolution_clock> w;

   int count = 0;

   while(boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() < 200)
   {
      Float val = generate_random_float<Float>();
      do_round_trip<Float, Rat>(val);
      do_round_trip<Float, Rat>(Float(-val));
      do_round_trip<Float, Rat>(Float(1/val));
      do_round_trip<Float, Rat>(Float(-1/val));
      count += 4;
      if(boost::detail::test_errors() > 100)
         break;
   }

   std::cout << "Execution time = " << boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() << "s" << std::endl;
   std::cout << "Total values tested: " << count << std::endl;
#endif
}

template <class Int>
Int generate_random_int()
{
   static boost::random::mt19937 gen;
   static boost::random::uniform_int_distribution<boost::random::mt19937::result_type> d(1, 20);

   int lim;
   Int cppi(0);

   lim = d(gen);

   for(int i = 0; i < lim; ++i)
   {
      cppi *= (gen.max)();
      cppi += gen();
   }
   return cppi;
}

template <class Float, class Rat>
void test_random_rationals()
{
#ifndef BOOST_MP_NO_CXX11_EXPLICIT_CONVERSION_OPERATORS
   std::cout << "Testing types " << typeid(Float).name() << " <<==>> " << typeid(Rat).name() << std::endl;
   std::cout << "digits = " << std::numeric_limits<Float>::digits << std::endl;
   std::cout << "digits10 = " << std::numeric_limits<Float>::digits10 << std::endl;
#ifndef BOOST_NO_CXX11_NUMERIC_LIMITS
   std::cout << "max_digits10 = " << std::numeric_limits<Float>::max_digits10 << std::endl;
#endif

   typedef typename boost::multiprecision::component_type<Rat>::type i_type;
   stopwatch<boost::chrono::high_resolution_clock> w;

   int count = 0;

   while(boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() < 200)
   {
      Rat rat(generate_random_int<i_type>(), generate_random_int<i_type>());
      Float f(rat);
      Rat new_rat(f); // rounded value
      int c = new_rat.compare(rat);
      if(c < 0)
      {
         // If f was rounded down, next float up must be above the original value:
         f = boost::math::float_next(f);
         new_rat.assign(f);
         BOOST_CHECK(new_rat >= rat);
      }
      else if(c > 0)
      {
         // If f was rounded up, next float down must be below the original value:
         f = boost::math::float_prior(f);
         new_rat.assign(f);
         BOOST_CHECK(new_rat <= rat);
      }
      else
      {
         // Values were equal... nothing to test.
      }
      if(boost::detail::test_errors() > 100)
         break;
   }

   std::cout << "Execution time = " << boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() << "s" << std::endl;
   std::cout << "Total values tested: " << count << std::endl;
#endif
}

#if defined(TEST2)

void double_spot_tests()
{
   boost::multiprecision::cpp_rational rat = 1;
   boost::multiprecision::cpp_rational twiddle(boost::multiprecision::cpp_int(1), boost::multiprecision::cpp_int(boost::multiprecision::cpp_int(1) << 54));
   rat += boost::multiprecision::cpp_rational(boost::multiprecision::cpp_int(1), boost::multiprecision::cpp_int(boost::multiprecision::cpp_int(1) << 50));

   double d = rat.convert_to<double>();

   rat += twiddle;
   BOOST_CHECK_EQUAL(d, rat.convert_to<double>());
   rat += twiddle;
   // tie: round to even rounds down
   BOOST_CHECK_EQUAL(d, rat.convert_to<double>());
   rat += twiddle;
   BOOST_CHECK_NE(d, rat.convert_to<double>());
   rat -= twiddle;
   BOOST_CHECK_EQUAL(d, rat.convert_to<double>());
   rat += boost::multiprecision::cpp_rational(boost::multiprecision::cpp_int(1), boost::multiprecision::cpp_int(boost::multiprecision::cpp_int(1) << 52));
   // tie, but last bit is now a 1 so we round up:
   BOOST_CHECK_NE(d, rat.convert_to<double>());

}

#endif

int main()
{
   using namespace boost::multiprecision;
#if defined(TEST1) && !defined(BOOST_MSVC)
   test_round_trip<number<cpp_bin_float<113, digit_base_2, void, boost::int16_t> >, cpp_rational>();
#elif defined(TEST2)
   double_spot_tests();
   test_round_trip<double, cpp_rational>();
#elif defined(TEST3) && !defined(BOOST_MSVC)
   test_random_rationals<number<cpp_bin_float<113, digit_base_2, void, boost::int16_t> >, cpp_rational>();
#elif defined(TEST4)
   test_random_rationals<double, cpp_rational>();
#elif defined(TEST5)
   // This does not work: gmp does not correctly round integer to float or
   // rational to float conversions:
   test_round_trip<double, mpq_rational>();
#elif defined(TEST6)
   test_round_trip<mpfr_float_100, mpq_rational>();
#elif defined(TEST7)
   test_random_rationals<mpfr_float_100, mpq_rational>();
#elif defined(TEST8)
   test_random_rationals<double, mpq_rational>();
#endif
   return boost::report_errors();
}

