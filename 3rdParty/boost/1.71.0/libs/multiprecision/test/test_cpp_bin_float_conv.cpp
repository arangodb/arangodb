///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/cstdint.hpp>


template <class T>
T generate_random()
{
   typedef int e_type;
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

   static boost::random::uniform_int_distribution<e_type> ui(-20, 20);
   return ldexp(val, ui(gen));
}

template <class T>
void check_round(const T& val, bool check_extended = false)
{
   double d1 = val.template convert_to<double>();
   double d2 = boost::math::nextafter(d1, d1 < val ? DBL_MAX : -DBL_MAX);
   T diff1 = abs(d1 - val);
   T diff2 = abs(d2 - val);
   if(diff2 < diff1)
   {
      // Some debugging code here...
      std::cout << val.str() << std::endl;
      std::cout << std::setprecision(18);
      std::cout << d1 << std::endl;
      std::cout << d2 << std::endl;
      std::cout << diff1 << std::endl;
      std::cout << diff2 << std::endl;
      d1 = val.template convert_to<double>();
   }
   BOOST_CHECK(diff2 >= diff1);
   BOOST_CHECK_EQUAL(boost::math::signbit(val), boost::math::signbit(d1));

   float f1 = val.template convert_to<float>();
   float f2 = boost::math::nextafter(f1, f1 < val ? FLT_MAX : -FLT_MAX);
   BOOST_CHECK(((abs(f1 - val) <= abs(f2 - val))));
   BOOST_CHECK_EQUAL(boost::math::signbit(val), boost::math::signbit(f1));

#if !defined(BOOST_MATH_NO_LONG_DOUBLE_MATH_FUNCTIONS)

   if(check_extended)
   {
      //
      // We should check long double as well:
      //
      long double l1 = val.template convert_to<long double>();
      long double l2 = boost::math::nextafter(d1, d1 < val ? LDBL_MAX : -LDBL_MAX);
      diff1 = abs(l1 - val);
      diff2 = abs(l2 - val);
      if(diff2 < diff1)
      {
         // Some debugging code here...
         std::cout << val.str() << std::endl;
         std::cout << std::setprecision(18);
         std::cout << l1 << std::endl;
         std::cout << l2 << std::endl;
         std::cout << diff1 << std::endl;
         std::cout << diff2 << std::endl;
         l1 = val.template convert_to<long double>();
      }
      BOOST_CHECK(diff2 >= diff1);
      BOOST_CHECK_EQUAL(boost::math::signbit(val), boost::math::signbit(l1));
   }

#endif

}


int main()
{
   using namespace boost::multiprecision;

   cpp_int i(20);
   cpp_bin_float_50 f50(i);
   BOOST_CHECK_EQUAL(f50, 20);
   f50 = i.convert_to<cpp_bin_float_50>();
   BOOST_CHECK_EQUAL(f50, 20);

   int1024_t i1024(45);
   cpp_bin_float_100 f100(i1024);
   BOOST_CHECK_EQUAL(f100, 45);
   f100 = i1024.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100, 45);

   uint1024_t ui1024(55);
   cpp_bin_float_100 f100b(ui1024);
   BOOST_CHECK_EQUAL(f100b, 55);
   f100b = ui1024.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100b, 55);

   typedef number<cpp_int_backend<32, 32>, et_off> i32_t;
   i32_t i32(67);
   cpp_bin_float_100 f100c(i32);
   BOOST_CHECK_EQUAL(f100c, 67);
   f100c = i32.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100c, 67);

   typedef number<cpp_int_backend<32, 32, unsigned_magnitude>, et_off> ui32_t;
   ui32_t ui32(98);
   cpp_bin_float_100 f100d(ui32);
   BOOST_CHECK_EQUAL(f100d, 98);
   f100d = ui32.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100d, 98);

   typedef number<cpp_int_backend<64, 64>, et_off> i64_t;
   i64_t i64(67);
   cpp_bin_float_100 f100e(i64);
   BOOST_CHECK_EQUAL(f100e, 67);
   f100e = i64.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100e, 67);

   typedef number<cpp_int_backend<64, 64, unsigned_magnitude>, et_off> ui64_t;
   ui64_t ui64(98);
   cpp_bin_float_100 f100f(ui64);
   BOOST_CHECK_EQUAL(f100f, 98);
   f100f = ui64.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100f, 98);

   typedef number<cpp_int_backend<128, 128>, et_off> i128_t;
   i128_t i128(67);
   cpp_bin_float_100 f100g(i128);
   BOOST_CHECK_EQUAL(f100g, 67);
   f100g = i128.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100g, 67);

   typedef number<cpp_int_backend<128, 128, unsigned_magnitude>, et_off> ui128_t;
   ui128_t ui128(98);
   cpp_bin_float_100 f100h(ui128);
   BOOST_CHECK_EQUAL(f100h, 98);
   f100h = ui128.convert_to<cpp_bin_float_100>();
   BOOST_CHECK_EQUAL(f100h, 98);

   cpp_bin_float_quad q = (std::numeric_limits<float>::min)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), (std::numeric_limits<float>::min)());
   q = (std::numeric_limits<float>::max)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), (std::numeric_limits<float>::max)());
   q = (std::numeric_limits<float>::denorm_min)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), (std::numeric_limits<float>::denorm_min)());
   // See https://svn.boost.org/trac/boost/ticket/12512:
   boost::multiprecision::number<boost::multiprecision::backends::cpp_bin_float<128, boost::multiprecision::backends::digit_base_2, void, boost::int64_t> > ext_float("1e-646456978");
   BOOST_CHECK_EQUAL(ext_float.convert_to<float>(), 0);

   q = -(std::numeric_limits<float>::min)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), -(std::numeric_limits<float>::min)());
   q = -(std::numeric_limits<float>::max)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), -(std::numeric_limits<float>::max)());
   q = -(std::numeric_limits<float>::denorm_min)();
   BOOST_CHECK_EQUAL(q.convert_to<float>(), -(std::numeric_limits<float>::denorm_min)());
   // See https://svn.boost.org/trac/boost/ticket/12512:
   ext_float = boost::multiprecision::number<boost::multiprecision::backends::cpp_bin_float<128, boost::multiprecision::backends::digit_base_2, void, boost::int64_t> >("-1e-646456978");
   BOOST_CHECK_EQUAL(ext_float.convert_to<float>(), 0);
   ext_float = (std::numeric_limits<double>::max)();
   ext_float *= 16;
   if(std::numeric_limits<double>::has_infinity)
   {
      BOOST_CHECK_EQUAL(ext_float.convert_to<double>(), std::numeric_limits<double>::infinity());
   }
   else
   {
      BOOST_CHECK_EQUAL(ext_float.convert_to<double>(), (std::numeric_limits<double>::max)());
   }
   ext_float = -ext_float;
   if(std::numeric_limits<double>::has_infinity)
   {
      BOOST_CHECK_EQUAL(ext_float.convert_to<double>(), -std::numeric_limits<double>::infinity());
   }
   else
   {
      BOOST_CHECK_EQUAL(ext_float.convert_to<double>(), -(std::numeric_limits<double>::max)());
   }
   //
   // Check for double rounding when the result would be a denorm.
   // See https://svn.boost.org/trac/boost/ticket/12527
   //
   cpp_bin_float_50 r1 = ldexp(cpp_bin_float_50(0x8000000000000bffull), -63 - 1023);
   check_round(r1);
   r1 = -r1;
   check_round(r1);

   r1 = ldexp(cpp_bin_float_50(0x8000017f), -31 - 127);
   check_round(r1);
   r1 = -r1;
   check_round(r1);
   //
   // Check convertion to signed zero works OK:
   //
   r1 = -ldexp(cpp_bin_float_50(1), -3000);
   check_round(r1);
   r1 = 0;
   r1 = -r1;
   BOOST_CHECK(boost::math::signbit(r1));
   check_round(r1);
   //
   // Check boundary as the exponent drops below what a double can cope with
   // but we will be rounding up:
   //
   r1 = 3;
   r1 = ldexp(r1, std::numeric_limits<double>::min_exponent);
   do
   {
      check_round(r1);
      check_round(boost::math::float_next(r1));
      check_round(boost::math::float_prior(r1));
      r1 = ldexp(r1, -1);
   } while(ilogb(r1) > std::numeric_limits<double>::min_exponent - 5 - std::numeric_limits<double>::digits);
   r1 = -3;
   r1 = ldexp(r1, std::numeric_limits<double>::min_exponent);
   do
   {
      check_round(r1);
      check_round(boost::math::float_next(r1));
      check_round(boost::math::float_prior(r1));
      r1 = ldexp(r1, -1);
   } while(ilogb(r1) > std::numeric_limits<double>::min_exponent - 5 - std::numeric_limits<double>::digits);
   //
   // Again when not rounding up:
   //
   r1 = 1;
   r1 = ldexp(r1, std::numeric_limits<double>::min_exponent);
   do
   {
      check_round(r1);
      check_round(boost::math::float_next(r1));
      check_round(boost::math::float_prior(r1));
      r1 = ldexp(r1, -1);
   } while(ilogb(r1) > std::numeric_limits<double>::min_exponent - 5 - std::numeric_limits<double>::digits);
   r1 = -1;
   r1 = ldexp(r1, std::numeric_limits<double>::min_exponent);
   do
   {
      check_round(r1);
      check_round(boost::math::float_next(r1));
      check_round(boost::math::float_prior(r1));
      r1 = ldexp(r1, -1);
   } while(ilogb(r1) > std::numeric_limits<double>::min_exponent - 5 - std::numeric_limits<double>::digits);
   //
   // Test random conversions:
   //
   for(unsigned j = 0; j < 10000; ++j)
   {
      check_round(generate_random<cpp_bin_float_50>(), true);
   }

   return boost::report_errors();
}

