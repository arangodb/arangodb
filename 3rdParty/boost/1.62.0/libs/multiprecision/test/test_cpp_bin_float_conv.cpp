///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include <boost/array.hpp>
#include "test.hpp"

#include <boost/multiprecision/cpp_int.hpp>
#include <boost/multiprecision/cpp_bin_float.hpp>

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

   return boost::report_errors();
}



