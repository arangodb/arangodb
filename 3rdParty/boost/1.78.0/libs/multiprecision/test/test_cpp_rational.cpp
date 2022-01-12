///////////////////////////////////////////////////////////////
//  Copyright 2021 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

//
// Compare arithmetic results using fixed_int to GMP results.
//

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/gmp.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include "timer.hpp"
#include "test.hpp"

#ifdef _MSC_VER
#pragma warning(disable : 4127) //  Conditional expression is constant
#endif

#if !defined(TEST1) && !defined(TEST2) && !defined(TEST3) && !defined(TEST4) && !defined(TEST5) && !defined(TEST6)
#define TEST1
#define TEST2
#define TEST3
#define TEST4
#define TEST5
#define TEST6
#endif

template <class T>
T generate_random(unsigned bits_wanted)
{
   static boost::random::mt19937               gen;
   typedef boost::random::mt19937::result_type random_type;

   T        max_val;
   unsigned digits;
   if (std::numeric_limits<T>::is_bounded && (bits_wanted == (unsigned)std::numeric_limits<T>::digits))
   {
      max_val = (std::numeric_limits<T>::max)();
      digits  = std::numeric_limits<T>::digits;
   }
   else
   {
      max_val = T(1) << bits_wanted;
      digits  = bits_wanted;
   }

   unsigned bits_per_r_val = std::numeric_limits<random_type>::digits - 1;
   while ((random_type(1) << bits_per_r_val) > (gen.max)())
      --bits_per_r_val;

   unsigned terms_needed = digits / bits_per_r_val + 1;

   T val = 0;
   for (unsigned i = 0; i < terms_needed; ++i)
   {
      val *= (gen.max)();
      val += gen();
   }
   val %= max_val;
   return val;
}

template <class Number>
struct tester
{
   typedef Number                                         test_type;
   typedef typename test_type::backend_type::checked_type checked;

   unsigned     last_error_count;
   timer tim;

   boost::multiprecision::mpz_int a, b, c, d;
   int                            si;
   unsigned                       ui;
   boost::multiprecision::double_limb_type large_ui;
   test_type                      a1, b1, c1, d1;

   void t1()
   {
      //
      // Arithmetic, non-mixed:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());

      // positive x, y:
      z = x + y;
      z1 = x1 + y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x - y;
      z1 = x1 - y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x * y;
      z1 = x1 * y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x / y;
      z1 = x1 / y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      
      // negative y:
      y = -y;
      y1 = -y1;
      BOOST_CHECK(y < 0);
      z  = x + y;
      z1 = x1 + y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - y;
      z1 = x1 - y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * y;
      z1 = x1 * y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / y;
      z1 = x1 / y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      
      // negative x:
      x.swap(y);
      x1.swap(y1);
      BOOST_CHECK(x < 0);
      z  = x + y;
      z1 = x1 + y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - y;
      z1 = x1 - y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * y;
      z1 = x1 * y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / y;
      z1 = x1 / y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // negative x, y:
      y = -y;
      y1 = -y1;
      BOOST_CHECK(x < 0);
      BOOST_CHECK(y < 0);
      z  = x + y;
      z1 = x1 + y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - y;
      z1 = x1 - y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * y;
      z1 = x1 * y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / y;
      z1 = x1 / y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Inplace, negative x, y:
      BOOST_CHECK(x < 0);
      BOOST_CHECK(y < 0);
      z = x;
      z += y;
      z1 = x1;
      z1 += y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z -= y;
      z1 = x1;
      z1 -= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= y;
      z1 = x1;
      z1 *= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= z;
      z1 = x1;
      z1 *= z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z /= y;
      z1 = x1;
      z1 /= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Inplace, negative x:
      y  = -y;
      y1 = -y1;
      BOOST_CHECK(x < 0);
      z = x;
      z += y;
      z1 = x1;
      z1 += y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z -= y;
      z1 = x1;
      z1 -= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= y;
      z1 = x1;
      z1 *= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= z;
      z1 = x1;
      z1 *= z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z /= y;
      z1 = x1;
      z1 /= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Inplace, negative y:
      x.swap(y);
      x1.swap(y1);
      BOOST_CHECK(y < 0);
      z = x;
      z += y;
      z1 = x1;
      z1 += y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z -= y;
      z1 = x1;
      z1 -= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= y;
      z1 = x1;
      z1 *= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= z;
      z1 = x1;
      z1 *= z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z /= y;
      z1 = x1;
      z1 /= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Inplace, positive x, y:
      y = -y;
      y1 = -y1;
      BOOST_CHECK(x > 0);
      BOOST_CHECK(y > 0);
      z = x;
      z += y;
      z1 = x1;
      z1 += y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z -= y;
      z1 = x1;
      z1 -= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= y;
      z1 = x1;
      z1 *= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z *= z;
      z1 = x1;
      z1 *= z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z /= y;
      z1 = x1;
      z1 /= y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      BOOST_CHECK_EQUAL((x == y), (x1 == y1));
      BOOST_CHECK_EQUAL((x != y), (x1 != y1));
      BOOST_CHECK_EQUAL((x <= y), (x1 <= y1));
      BOOST_CHECK_EQUAL((x >= y), (x1 >= y1));
      BOOST_CHECK_EQUAL((x < y), (x1 < y1));
      BOOST_CHECK_EQUAL((x > y), (x1 > y1));

      z = x;
      z1 = x1;
      BOOST_CHECK_EQUAL((x == z), (x1 == z1));
      BOOST_CHECK_EQUAL((x != z), (x1 != z1));
      BOOST_CHECK_EQUAL((x <= z), (x1 <= z1));
      BOOST_CHECK_EQUAL((x >= z), (x1 >= z1));
      BOOST_CHECK_EQUAL((x < z), (x1 < z1));
      BOOST_CHECK_EQUAL((x > z), (x1 > z1));
   }

   void t2()
   {
      //
      // Mixed with signed integer:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());

      // Both positive:
      z = x + si;
      z1 = x1 + si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x - si;
      z1 = x1 - si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x * si;
      z1 = x1 * si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x / si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si + x;
      z1 = si + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si - x;
      z1 = si - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si * x;
      z1 = si * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si / x;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x = -x;
      x1 = -x1;
      z  = x + si;
      z1 = x1 + si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - si;
      z1 = x1 - si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * si;
      z1 = x1 * si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si + x;
      z1 = si + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si - x;
      z1 = si - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si * x;
      z1 = si * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si / x;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x and si both negative:
      z  = x + si;
      z1 = x1 + si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - si;
      z1 = x1 - si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * si;
      z1 = x1 * si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si + x;
      z1 = si + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si - x;
      z1 = si - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si * x;
      z1 = si * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si / x;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // si negative:
      x = -x;
      x1 = -x1;
      z  = x + si;
      z1 = x1 + si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - si;
      z1 = x1 - si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * si;
      z1 = x1 * si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si + x;
      z1 = si + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si - x;
      z1 = si - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si * x;
      z1 = si * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = si / x;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      si = -si;
      // Inplace:
      z = x;
      z1 = x1;
      z += si;
      z1 += si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = x;
      z1 = x1;
      z -= si;
      z1 -= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= si;
      z1 *= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // si negative:
      si = -si;
      z  = x;
      z1 = x1;
      z += si;
      z1 += si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= si;
      z1 -= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= si;
      z1 *= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Both negative:
      x  = -x;
      x1 = -x1;
      z  = x;
      z1 = x1;
      z += si;
      z1 += si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= si;
      z1 -= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= si;
      z1 *= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      si = -si;
      z  = x;
      z1 = x1;
      z += si;
      z1 += si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= si;
      z1 -= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= si;
      z1 *= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      BOOST_CHECK_EQUAL((x == si), (x1 == si));
      BOOST_CHECK_EQUAL((x != si), (x1 != si));
      BOOST_CHECK_EQUAL((x <= si), (x1 <= si));
      BOOST_CHECK_EQUAL((x >= si), (x1 >= si));
      BOOST_CHECK_EQUAL((x < si), (x1 < si));
      BOOST_CHECK_EQUAL((x > si), (x1 > si));

      z = si;
      z1 = si;
      BOOST_CHECK_EQUAL((x == si), (x1 == si));
      BOOST_CHECK_EQUAL((x != si), (x1 != si));
      BOOST_CHECK_EQUAL((x <= si), (x1 <= si));
      BOOST_CHECK_EQUAL((x >= si), (x1 >= si));
      BOOST_CHECK_EQUAL((x < si), (x1 < si));
      BOOST_CHECK_EQUAL((x > si), (x1 > si));
   }

   void t3()
   {
      //
      // Mixed with unsigned integer:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());

      // Both positive:
      z  = x + ui;
      z1 = x1 + ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - ui;
      z1 = x1 - ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * ui;
      z1 = x1 * ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / ui;
      z1 = x1 / ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui + x;
      z1 = ui + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui - x;
      z1 = ui - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui * x;
      z1 = ui * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui / x;
      z1 = ui / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x  = -x;
      x1 = -x1;
      z  = x + ui;
      z1 = x1 + ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - ui;
      z1 = x1 - ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * ui;
      z1 = x1 * ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / ui;
      z1 = x1 / ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui + x;
      z1 = ui + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui - x;
      z1 = ui - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui * x;
      z1 = ui * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = ui / x;
      z1 = ui / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      x = -x;
      x1 = -x1;
      // Inplace:
      z  = x;
      z1 = x1;
      z += ui;
      z1 += ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= ui;
      z1 -= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= ui;
      z1 *= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= ui;
      z1 /= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x  = -x;
      x1 = -x1;
      z  = x;
      z1 = x1;
      z += ui;
      z1 += ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= ui;
      z1 -= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= ui;
      z1 *= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= ui;
      z1 /= ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      x = -x;
      x1 = -x1;

      BOOST_CHECK_EQUAL((x == ui), (x1 == ui));
      BOOST_CHECK_EQUAL((x != ui), (x1 != ui));
      BOOST_CHECK_EQUAL((x <= ui), (x1 <= ui));
      BOOST_CHECK_EQUAL((x >= ui), (x1 >= ui));
      BOOST_CHECK_EQUAL((x < ui), (x1 < ui));
      BOOST_CHECK_EQUAL((x > ui), (x1 > ui));

      z  = ui;
      z1 = ui;
      BOOST_CHECK_EQUAL((x == ui), (x1 == ui));
      BOOST_CHECK_EQUAL((x != ui), (x1 != ui));
      BOOST_CHECK_EQUAL((x <= ui), (x1 <= ui));
      BOOST_CHECK_EQUAL((x >= ui), (x1 >= ui));
      BOOST_CHECK_EQUAL((x < ui), (x1 < ui));
      BOOST_CHECK_EQUAL((x > ui), (x1 > ui));
   }

   void t4()
   {
      //
      // Mixed with unsigned long integer:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());

      // Both positive:
      z  = x + large_ui;
      z1 = x1 + large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - large_ui;
      z1 = x1 - large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * large_ui;
      z1 = x1 * large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / large_ui;
      z1 = x1 / large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui + x;
      z1 = large_ui + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui - x;
      z1 = large_ui - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui * x;
      z1 = large_ui * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui / x;
      z1 = large_ui / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x  = -x;
      x1 = -x1;
      z  = x + large_ui;
      z1 = x1 + large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - large_ui;
      z1 = x1 - large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * large_ui;
      z1 = x1 * large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / large_ui;
      z1 = x1 / large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui + x;
      z1 = large_ui + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui - x;
      z1 = large_ui - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui * x;
      z1 = large_ui * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = large_ui / x;
      z1 = large_ui / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      x  = -x;
      x1 = -x1;
      // Inplace:
      z  = x;
      z1 = x1;
      z += large_ui;
      z1 += large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= large_ui;
      z1 -= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= large_ui;
      z1 *= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= large_ui;
      z1 /= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x  = -x;
      x1 = -x1;
      z  = x;
      z1 = x1;
      z += large_ui;
      z1 += large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= large_ui;
      z1 -= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= large_ui;
      z1 *= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= large_ui;
      z1 /= large_ui;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      x  = -x;
      x1 = -x1;

      BOOST_CHECK_EQUAL((x == large_ui), (x1 == large_ui));
      BOOST_CHECK_EQUAL((x != large_ui), (x1 != large_ui));
      BOOST_CHECK_EQUAL((x <= large_ui), (x1 <= large_ui));
      BOOST_CHECK_EQUAL((x >= large_ui), (x1 >= large_ui));
      BOOST_CHECK_EQUAL((x < large_ui), (x1 < large_ui));
      BOOST_CHECK_EQUAL((x > large_ui), (x1 > large_ui));

      z  = large_ui;
      z1 = large_ui;
      BOOST_CHECK_EQUAL((x == large_ui), (x1 == large_ui));
      BOOST_CHECK_EQUAL((x != large_ui), (x1 != large_ui));
      BOOST_CHECK_EQUAL((x <= large_ui), (x1 <= large_ui));
      BOOST_CHECK_EQUAL((x >= large_ui), (x1 >= large_ui));
      BOOST_CHECK_EQUAL((x < large_ui), (x1 < large_ui));
      BOOST_CHECK_EQUAL((x > large_ui), (x1 > large_ui));
   }

   void t5()
   {
      //
      // Special cases:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());

      BOOST_CHECK_EQUAL(x1 * 0, 0);
      BOOST_CHECK_EQUAL(x1 * 1, x1);
      BOOST_CHECK_EQUAL(x1 * -1, -x1);

      z = x * y;
      z1 = x1;
      x1 = x1 * y1;
      BOOST_CHECK_EQUAL(z.str(), x1.str());
      x1 = z1;

      x1 = y1 * x1;
      BOOST_CHECK_EQUAL(z.str(), x1.str());
      x1 = z1;

      z = x * si;
      x1 = x1 * si;
      BOOST_CHECK_EQUAL(z.str(), x1.str());
      x1 = z1;

      x1 = si * x1;
      BOOST_CHECK_EQUAL(z.str(), x1.str());
      x1 = z1;

      z1 = x1;
      z1 *= 0;
      BOOST_CHECK_EQUAL(z1, 0);
      z1 = x1;
      z1 *= 1;
      BOOST_CHECK_EQUAL(z1, x1);
      z1 = x1;
      z1 *= -1;
      BOOST_CHECK_EQUAL(z1, -x1);

      z1 = x1 / x1;
      BOOST_CHECK_EQUAL(z1, 1);
      z1 = x1 / -x1;
      BOOST_CHECK_EQUAL(z1, -1);
      z = x / y;
      z1 = x1;
      z1 = z1 / y1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z1 = y1;
      z1 = x1 / z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      z = x / si;
      z1 = x1;
      z1 = z1 / si;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z = si / x;
      z1 = x1;
      z1 = si / z1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      BOOST_CHECK_THROW(z1 = x1 / 0, std::overflow_error);
      z1= x1;
      BOOST_CHECK_THROW(z1 /= 0, std::overflow_error);
      z1 = x1;
      z1 /= 1;
      BOOST_CHECK_EQUAL(z1, x1);
      z1 /= -1;
      BOOST_CHECK_EQUAL(z1, -x1);
      z1 = x1 / 1;
      BOOST_CHECK_EQUAL(z1, x1);
      z1 = x1 / -1;
      BOOST_CHECK_EQUAL(z1, -x1);

      z1 = 0;
      BOOST_CHECK_EQUAL(z1 * si, 0);
      BOOST_CHECK_EQUAL(z1 / si, 0);
      BOOST_CHECK_EQUAL(z1 + si, si);
      BOOST_CHECK_EQUAL(z1 - si, -si);
      z1 *= si;
      BOOST_CHECK_EQUAL(z1, 0);
      z1 /= si;
      BOOST_CHECK_EQUAL(z1, 0);
      z1 += si;
      BOOST_CHECK_EQUAL(z1, si);
      z1 = 0;
      z1 -= si;
      BOOST_CHECK_EQUAL(z1, -si);

      z1 = si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z1, 1);
      z1 = si;
      z1 /= -si;
      BOOST_CHECK_EQUAL(z1, -1);
      z1 = -si;
      z1 /= si;
      BOOST_CHECK_EQUAL(z1, -1);
      z1 = -si;
      z1 /= -si;
      BOOST_CHECK_EQUAL(z1, 1);

      x1 = si;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z1, 1);
      x1 = si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z1, 1);
      x1 = -si;
      z1 = si / x1;
      BOOST_CHECK_EQUAL(z1, -1);
      x1 = -si;
      z1 = x1 / si;
      BOOST_CHECK_EQUAL(z1, -1);
      x1 = -si;
      z1 = -si / x1;
      BOOST_CHECK_EQUAL(z1, 1);
      x1 = -si;
      z1 = x1 / -si;
      BOOST_CHECK_EQUAL(z1, 1);
      x1 = si;
      z1 = -si / x1;
      BOOST_CHECK_EQUAL(z1, -1);
      x1 = si;
      z1 = x1 / -si;
      BOOST_CHECK_EQUAL(z1, -1);
   }

   void t6()
   {
      //
      // Mixed with signed integer:
      //
      boost::multiprecision::mpq_rational x(a, b), y(c, d), z;
      boost::multiprecision::cpp_rational x1(a1, b1), y1(c1, d1), z1;

      boost::multiprecision::mpz_int bi = generate_random<boost::multiprecision::mpz_int>(1000);
      boost::multiprecision::cpp_int bi1(bi.str());

      BOOST_CHECK_EQUAL(x.str(), x1.str());
      BOOST_CHECK_EQUAL(y.str(), y1.str());
      BOOST_CHECK_EQUAL(bi.str(), bi1.str());

      // Both positive:
      z  = x + bi;
      z1 = x1 + bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - bi;
      z1 = x1 - bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * bi;
      z1 = x1 * bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / bi;
      z1 = x1 / bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi + x;
      z1 = bi1 + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi - x;
      z1 = bi1 - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi * x;
      z1 = bi1 * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi / x;
      z1 = bi1 / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      x  = -x;
      x1 = -x1;
      z  = x + bi;
      z1 = x1 + bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - bi;
      z1 = x1 - bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * bi;
      z1 = x1 * bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / bi;
      z1 = x1 / bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi + x;
      z1 = bi1 + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi - x;
      z1 = bi1 - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi * x;
      z1 = bi1 * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi / x;
      z1 = bi1 / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x and bi both negative:
      z  = x + bi;
      z1 = x1 + bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - bi;
      z1 = x1 - bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * bi;
      z1 = x1 * bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / bi;
      z1 = x1 / bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi + x;
      z1 = bi1 + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi - x;
      z1 = bi1 - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi * x;
      z1 = bi1 * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi / x;
      z1 = bi1 / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // bi negative:
      x  = -x;
      x1 = -x1;
      z  = x + bi;
      z1 = x1 + bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x - bi;
      z1 = x1 - bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x * bi;
      z1 = x1 * bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x / bi;
      z1 = x1 / bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi + x;
      z1 = bi1 + x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi - x;
      z1 = bi1 - x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi * x;
      z1 = bi1 * x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = bi / x;
      z1 = bi1 / x1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      bi = -bi;
      bi1 = -bi1;
      // Inplace:
      z  = x;
      z1 = x1;
      z += bi;
      z1 += bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= bi;
      z1 -= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= bi;
      z1 *= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= bi;
      z1 /= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // bi negative:
      bi = -bi;
      bi1 = -bi1;
      z  = x;
      z1 = x1;
      z += bi;
      z1 += bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= bi;
      z1 -= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= bi;
      z1 *= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= bi;
      z1 /= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // Both negative:
      x  = -x;
      x1 = -x1;
      z  = x;
      z1 = x1;
      z += bi;
      z1 += bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= bi;
      z1 -= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= bi;
      z1 *= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= bi;
      z1 /= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      // x negative:
      bi = -bi;
      bi1 = -bi1;
      z  = x;
      z1 = x1;
      z += bi;
      z1 += bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z -= bi;
      z1 -= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z *= bi;
      z1 *= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());
      z  = x;
      z1 = x1;
      z /= bi;
      z1 /= bi1;
      BOOST_CHECK_EQUAL(z.str(), z1.str());

      BOOST_CHECK_EQUAL((x == bi), (x1 == bi1));
      BOOST_CHECK_EQUAL((x != bi), (x1 != bi1));
      BOOST_CHECK_EQUAL((x <= bi), (x1 <= bi1));
      BOOST_CHECK_EQUAL((x >= bi), (x1 >= bi1));
      BOOST_CHECK_EQUAL((x < bi), (x1 < bi1));
      BOOST_CHECK_EQUAL((x > bi), (x1 > bi1));

      z  = bi;
      z1 = bi1;
      BOOST_CHECK_EQUAL((x == bi), (x1 == bi1));
      BOOST_CHECK_EQUAL((x != bi), (x1 != bi1));
      BOOST_CHECK_EQUAL((x <= bi), (x1 <= bi1));
      BOOST_CHECK_EQUAL((x >= bi), (x1 >= bi1));
      BOOST_CHECK_EQUAL((x < bi), (x1 < bi1));
      BOOST_CHECK_EQUAL((x > bi), (x1 > bi1));
   }

   void test()
   {
      using namespace boost::multiprecision;

      last_error_count = 0;

      BOOST_CHECK_EQUAL(Number(), 0);

      for (int i = 0; i < 10000; ++i)
      {
         a = generate_random<mpz_int>(1000);
         b = generate_random<mpz_int>(1000);
         c = generate_random<mpz_int>(1000);
         d = generate_random<mpz_int>(1000);

         si       = generate_random<int>(std::numeric_limits<int>::digits - 2);
         ui       = generate_random<unsigned>(std::numeric_limits<unsigned>::digits - 2);
         large_ui = generate_random<boost::multiprecision::double_limb_type>(std::numeric_limits<boost::multiprecision::double_limb_type>::digits - 2);

         a1 = static_cast<test_type>(a.str());
         b1 = static_cast<test_type>(b.str());
         c1 = static_cast<test_type>(c.str());
         d1 = static_cast<test_type>(d.str());

         t1();
         t2();
         t3();
         t4();
         t5();
         t6();

         if (last_error_count != (unsigned)boost::detail::test_errors())
         {
            last_error_count = boost::detail::test_errors();
            std::cout << std::hex << std::showbase;

            std::cout << "a    = " << a << std::endl;
            std::cout << "a1   = " << a1 << std::endl;
            std::cout << "b    = " << b << std::endl;
            std::cout << "b1   = " << b1 << std::endl;
            std::cout << "c    = " << c << std::endl;
            std::cout << "c1   = " << c1 << std::endl;
            std::cout << "d    = " << d << std::endl;
            std::cout << "d1   = " << d1 << std::endl;
            std::cout << "a + b   = " << a + b << std::endl;
            std::cout << "a1 + b1 = " << a1 + b1 << std::endl;
            std::cout << std::dec;
            std::cout << "a - b   = " << a - b << std::endl;
            std::cout << "a1 - b1 = " << a1 - b1 << std::endl;
            std::cout << "-a + b   = " << mpz_int(-a) + b << std::endl;
            std::cout << "-a1 + b1 = " << test_type(-a1) + b1 << std::endl;
            std::cout << "-a - b   = " << mpz_int(-a) - b << std::endl;
            std::cout << "-a1 - b1 = " << test_type(-a1) - b1 << std::endl;
            std::cout << "c*d    = " << c * d << std::endl;
            std::cout << "c1*d1  = " << c1 * d1 << std::endl;
            std::cout << "b*c    = " << b * c << std::endl;
            std::cout << "b1*c1  = " << b1 * c1 << std::endl;
            std::cout << "a/b    = " << a / b << std::endl;
            std::cout << "a1/b1  = " << a1 / b1 << std::endl;
            std::cout << "a/d    = " << a / d << std::endl;
            std::cout << "a1/d1  = " << a1 / d1 << std::endl;
            std::cout << "a%b    = " << a % b << std::endl;
            std::cout << "a1%b1  = " << a1 % b1 << std::endl;
            std::cout << "a%d    = " << a % d << std::endl;
            std::cout << "a1%d1  = " << a1 % d1 << std::endl;
         }

         //
         // Check to see if test is taking too long.
         // Tests run on the compiler farm time out after 300 seconds,
         // so don't get too close to that:
         //
#ifndef CI_SUPPRESS_KNOWN_ISSUES
         if (tim.elapsed() > 200)
#else
         if (tim.elapsed() > 25)
#endif
         {
            std::cout << "Timeout reached, aborting tests now....\n";
            break;
         }
      }
   }
};

int main()
{
   using namespace boost::multiprecision;

   tester<cpp_int> t1;
   t1.test();
   return boost::report_errors();
}
