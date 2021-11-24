// Copyright John Maddock 2013.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_bin_float.hpp>

#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/chrono.hpp>
#include "test.hpp"
#include <boost/array.hpp>
#include <iostream>
#include <iomanip>

#ifdef BOOST_MSVC
#pragma warning(disable : 4127)
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

void print_flags(std::ios_base::fmtflags f)
{
   std::cout << "Formatting flags were: ";
   if (f & std::ios_base::scientific)
      std::cout << "scientific ";
   if (f & std::ios_base::fixed)
      std::cout << "fixed ";
   if (f & std::ios_base::showpoint)
      std::cout << "showpoint ";
   if (f & std::ios_base::showpos)
      std::cout << "showpos ";
   std::cout << std::endl;
}

template <class T>
void test()
{
   typedef T                                mp_t;
   boost::array<std::ios_base::fmtflags, 9> f =
       {{std::ios_base::fmtflags(0), std::ios_base::showpoint, std::ios_base::showpos, std::ios_base::scientific, std::ios_base::scientific | std::ios_base::showpos,
         std::ios_base::scientific | std::ios_base::showpoint, std::ios_base::fixed, std::ios_base::fixed | std::ios_base::showpoint,
         std::ios_base::fixed | std::ios_base::showpos}};

   boost::array<boost::array<const char*, 13 * 9>, 40> string_data = {{
#include "libs/multiprecision/test/string_data.ipp"
   }};

   double num   = 123456789.0;
   double denom = 1;
   double val   = num;
   for (unsigned j = 0; j < 40; ++j)
   {
      unsigned col = 0;
      for (unsigned prec = 1; prec < 14; ++prec)
      {
         for (unsigned i = 0; i < f.size(); ++i, ++col)
         {
            std::stringstream ss;
            ss.precision(prec);
            ss.flags(f[i]);
            ss << mp_t(val);
            const char* expect = string_data[j][col];
            if (ss.str() != expect)
            {
               std::cout << std::setprecision(20) << "Testing value " << val << std::endl;
               print_flags(f[i]);
               std::cout << "Precision is: " << prec << std::endl;
               std::cout << "Got:      " << ss.str() << std::endl;
               std::cout << "Expected: " << expect << std::endl;
               ++boost::detail::test_errors();
               mp_t(val).str(prec, f[i]); // for debugging
            }
         }
      }
      num = -num;
      if (j & 1)
         denom *= 8;
      val = num / denom;
   }

   boost::array<const char*, 13 * 9> zeros =
       {{"0", "0.", "+0", "0.0e+00", "+0.0e+00", "0.0e+00", "0.0", "0.0", "+0.0", "0", "0.0", "+0", "0.00e+00", "+0.00e+00", "0.00e+00", "0.00", "0.00", "+0.00", "0", "0.00", "+0", "0.000e+00", "+0.000e+00", "0.000e+00", "0.000", "0.000", "+0.000", "0", "0.000", "+0", "0.0000e+00", "+0.0000e+00", "0.0000e+00", "0.0000", "0.0000", "+0.0000", "0", "0.0000", "+0", "0.00000e+00", "+0.00000e+00", "0.00000e+00", "0.00000", "0.00000", "+0.00000", "0", "0.00000", "+0", "0.000000e+00", "+0.000000e+00", "0.000000e+00", "0.000000", "0.000000", "+0.000000", "0", "0.000000", "+0", "0.0000000e+00", "+0.0000000e+00", "0.0000000e+00", "0.0000000", "0.0000000", "+0.0000000", "0", "0.0000000", "+0", "0.00000000e+00", "+0.00000000e+00", "0.00000000e+00", "0.00000000", "0.00000000", "+0.00000000", "0", "0.00000000", "+0", "0.000000000e+00", "+0.000000000e+00", "0.000000000e+00", "0.000000000", "0.000000000", "+0.000000000", "0", "0.000000000", "+0", "0.0000000000e+00", "+0.0000000000e+00", "0.0000000000e+00", "0.0000000000", "0.0000000000", "+0.0000000000", "0", "0.0000000000", "+0", "0.00000000000e+00", "+0.00000000000e+00", "0.00000000000e+00", "0.00000000000", "0.00000000000", "+0.00000000000", "0", "0.00000000000", "+0", "0.000000000000e+00", "+0.000000000000e+00", "0.000000000000e+00", "0.000000000000", "0.000000000000", "+0.000000000000", "0", "0.000000000000", "+0", "0.0000000000000e+00", "+0.0000000000000e+00", "0.0000000000000e+00", "0.0000000000000", "0.0000000000000", "+0.0000000000000"}};

   unsigned col = 0;
   val          = 0;
   for (unsigned prec = 1; prec < 14; ++prec)
   {
      for (unsigned i = 0; i < f.size(); ++i, ++col)
      {
         std::stringstream ss;
         ss.precision(prec);
         ss.flags(f[i]);
         ss << mp_t(val);
         const char* expect = zeros[col];
         if (ss.str() != expect)
         {
            std::cout << std::setprecision(20) << "Testing value " << val << std::endl;
            print_flags(f[i]);
            std::cout << "Precision is: " << prec << std::endl;
            std::cout << "Got:      " << ss.str() << std::endl;
            std::cout << "Expected: " << expect << std::endl;
            ++boost::detail::test_errors();
            mp_t(val).str(prec, f[i]); // for debugging
         }
      }
   }

   if (std::numeric_limits<mp_t>::has_infinity)
   {
      T val = std::numeric_limits<T>::infinity();
      BOOST_CHECK_EQUAL(val.str(), "inf");
      BOOST_CHECK_EQUAL(val.str(0, std::ios_base::showpos), "+inf");
      val = -val;
      BOOST_CHECK_EQUAL(val.str(), "-inf");
      BOOST_CHECK_EQUAL(val.str(0, std::ios_base::showpos), "-inf");

      val = static_cast<T>("inf");
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      val = static_cast<T>("+inf");
      BOOST_CHECK_EQUAL(val, std::numeric_limits<T>::infinity());
      val = static_cast<T>("-inf");
      BOOST_CHECK_EQUAL(val, -std::numeric_limits<T>::infinity());
   }
   if (std::numeric_limits<mp_t>::has_quiet_NaN)
   {
      T val = std::numeric_limits<T>::quiet_NaN();
      BOOST_CHECK_EQUAL(val.str(), "nan");
      val = static_cast<T>("nan");
      BOOST_CHECK((boost::math::isnan)(val));
   }
   //
   // Min and max values:
   //
   T t((std::numeric_limits<T>::max)().str(std::numeric_limits<T>::max_digits10, std::ios_base::scientific));
   BOOST_CHECK_EQUAL(t, (std::numeric_limits<T>::max)());
   t = T((std::numeric_limits<T>::min)().str(std::numeric_limits<T>::max_digits10, std::ios_base::scientific));
   BOOST_CHECK_EQUAL(t, (std::numeric_limits<T>::min)());
   t = T((std::numeric_limits<T>::lowest)().str(std::numeric_limits<T>::max_digits10, std::ios_base::scientific));
   BOOST_CHECK_EQUAL(t, (std::numeric_limits<T>::lowest)());
}

template <class T>
T generate_random()
{
   typedef typename T::backend_type::exponent_type e_type;
   static boost::random::mt19937                   gen;
   T                                               val      = gen();
   T                                               prev_val = -1;
   while (val != prev_val)
   {
      val *= (gen.max)();
      prev_val = val;
      val += gen();
   }
   e_type e;
   val = frexp(val, &e);

   static boost::random::uniform_int_distribution<e_type> ui(0, std::numeric_limits<T>::max_exponent);
   return ldexp(val, ui(gen));
}

template <class T>
void do_round_trip(const T& val, std::ios_base::fmtflags f)
{
   std::stringstream ss;
   ss << std::setprecision(std::numeric_limits<T>::max_digits10);
   ss.flags(f);
   ss << val;
   T new_val = static_cast<T>(ss.str());
   BOOST_CHECK_EQUAL(new_val, val);
   new_val = static_cast<T>(val.str(0, f));
   BOOST_CHECK_EQUAL(new_val, val);
}

template <class T>
void do_round_trip(const T& val)
{
   do_round_trip(val, std::ios_base::fmtflags(0));
   do_round_trip(val, std::ios_base::fmtflags(std::ios_base::scientific));
   if ((fabs(val) > 1) && (fabs(val) < 1e100))
      do_round_trip(val, std::ios_base::fmtflags(std::ios_base::fixed));

   static int error_count = 0;

   if (error_count != boost::detail::test_errors())
   {
      error_count = boost::detail::test_errors();
      std::cout << "Errors occurred while testing value....";
      if (val.backend().sign())
         std::cout << "-";
      std::cout << boost::multiprecision::cpp_int(val.backend().bits()) << "e" << val.backend().exponent() << std::endl;
   }
}

template <class T>
void test_round_trip()
{
   std::cout << "Testing type " << typeid(T).name() << std::endl;
   std::cout << "digits = " << std::numeric_limits<T>::digits << std::endl;
   std::cout << "digits10 = " << std::numeric_limits<T>::digits10 << std::endl;
   std::cout << "max_digits10 = " << std::numeric_limits<T>::max_digits10 << std::endl;

   stopwatch<boost::chrono::high_resolution_clock> w;

#ifndef CI_SUPPRESS_KNOWN_ISSUES
   while (boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() < 200)
#else
   while (boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() < 50)
#endif
   {
      T val = generate_random<T>();
      do_round_trip(val);
      do_round_trip(T(-val));
      do_round_trip(T(1 / val));
      do_round_trip(T(-1 / val));

      if (boost::detail::test_errors() > 200)
         break; // escape if there are too many errors.
   }

   std::cout << "Execution time = " << boost::chrono::duration_cast<boost::chrono::duration<double> >(w.elapsed()).count() << "s" << std::endl;
}

#if !defined(TEST1) && !defined(TEST2)
#define TEST1
#define TEST2
#endif

int main()
{
   using namespace boost::multiprecision;
#ifdef TEST1
   test<number<cpp_bin_float<113, digit_base_2> > >();
   test_round_trip<number<cpp_bin_float<113, digit_base_2> > >();
#endif
#ifdef TEST2
   test<number<cpp_bin_float<53, digit_base_2> > >();
   test_round_trip<number<cpp_bin_float<53, digit_base_2> > >();
#endif
   return boost::report_errors();
}
