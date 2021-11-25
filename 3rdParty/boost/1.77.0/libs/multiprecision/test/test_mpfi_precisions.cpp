///////////////////////////////////////////////////////////////
//  Copyright Christopher Kormanyos 2002 - 2011.
//  Copyright 2011 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt
//
// This work is based on an earlier work:
// "Algorithm 910: A Portable C++ Multiple-Precision System for Special-Function Calculations",
// in ACM TOMS, {VOL 37, ISSUE 4, (February 2011)} (C) ACM, 2011. http://doi.acm.org/10.1145/1916461.1916469

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include "test.hpp"

#include <boost/multiprecision/mpfi.hpp>

template <class T>
T make_rvalue_copy(const T a)
{
   return a;
}

int main()
{
   using namespace boost::multiprecision;
   //
   // Test change of default precision:
   //
   mpfi_float::default_precision(100);
   mpfi_float a("0.1");
   BOOST_CHECK_EQUAL(a.precision(), 100);
   mpfi_float::default_precision(20);
   {
      // test assignment from lvalue:
      mpfi_float b(2);
      BOOST_CHECK_EQUAL(b.precision(), 20);
      b = a;
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
   {
      // test assignment from rvalue:
      mpfi_float b(2);
      BOOST_CHECK_EQUAL(b.precision(), 20);
      b = make_rvalue_copy(a);
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
   mpfi_float::default_precision(20);
   {
      // test construct from lvalue:
      mpfi_float b(a);
      BOOST_CHECK_EQUAL(b.precision(), 100);
   }
   {
      // test construct from rvalue:
      mpfi_float b(make_rvalue_copy(a));
      BOOST_CHECK_EQUAL(b.precision(), 100);
   }
   {
      mpfi_float f150(2, 2, 150);
      BOOST_CHECK_EQUAL(f150.precision(), 150);
   }
   {
      mpfi_float f150("1.2", "1.2", 150);
      BOOST_CHECK_EQUAL(f150.precision(), 150);
   }
   //
   // From https://github.com/boostorg/multiprecision/issues/65
   //
   {
      mpfi_float a(2);
      a.precision(100);
      BOOST_CHECK_EQUAL(a, 2);
      BOOST_CHECK_EQUAL(a.precision(), 100);
   }

   //
   // string_view with explicit precision:
   //
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
   {
      std::string      s("222");
      std::string_view v(s.c_str(), 1);
      mpfi_float       f(v, v, 100);
      BOOST_CHECK_EQUAL(f, 2);
      BOOST_CHECK_EQUAL(f.precision(), 100);
   }
#endif
   // Swap:
   {
      mpfi_float x(2, 2, 100); // 100 digits precision.
      mpfi_float y(3, 3, 50);  // 50 digits precision.
      swap(x, y);
      BOOST_CHECK_EQUAL(x, 3);
      BOOST_CHECK_EQUAL(y, 2);
      BOOST_CHECK_EQUAL(x.precision(), 50);
      BOOST_CHECK_EQUAL(y.precision(), 100);
      x.swap(y);
      BOOST_CHECK_EQUAL(x, 2);
      BOOST_CHECK_EQUAL(y, 3);
      BOOST_CHECK_EQUAL(x.precision(), 100);
      BOOST_CHECK_EQUAL(y.precision(), 50);
      x = std::move(mpfi_float(y));
      BOOST_CHECK_EQUAL(x, y);
      BOOST_CHECK_EQUAL(x.precision(), y.precision());
   }
   {
      mpfi_float c(4), d(8), e(9), f;
      f = (c + d) * d / e;
      mpfi_float g((c + d) * d / e);
   }
   {
      mpfi_float::default_precision(100);
      mpfi_float f1;
      f1 = 3;
      BOOST_CHECK_EQUAL(f1.precision(), 100);
      f1 = 3.5;
      BOOST_CHECK_EQUAL(f1.precision(), 100);
      mpfi_float f2(3.5);
      BOOST_CHECK_EQUAL(f2.precision(), 100);
      mpfi_float f3("5.1");
      BOOST_CHECK_EQUAL(f3.precision(), 100);

      mpfi_float::default_precision(50);
      mpfi_float f4(f3, 50);
      BOOST_CHECK_EQUAL(f4.precision(), 50);
      f4.assign(f1, f4.precision());
      BOOST_CHECK_EQUAL(f4.precision(), 50);
   }
   {
      //
      // Check additional non-member functions,
      // see https://github.com/boostorg/multiprecision/issues/91
      //
      mpfi_float::default_precision(100);
      mpfi_float f1;
      f1 = 3;
      BOOST_CHECK_EQUAL(f1.precision(), 100);
      mpfi_float::default_precision(20);
      BOOST_CHECK_EQUAL(lower(f1).precision(), 100);
      BOOST_CHECK_EQUAL(upper(f1).precision(), 100);
      BOOST_CHECK_EQUAL(median(f1).precision(), 100);
      BOOST_CHECK_EQUAL(width(f1).precision(), 100);
      BOOST_CHECK_EQUAL(intersect(f1, f1).precision(), 100);
      BOOST_CHECK_EQUAL(hull(f1, f1).precision(), 100);

      BOOST_CHECK_EQUAL(asinh(f1).precision(), 100);
      BOOST_CHECK_EQUAL(acosh(f1).precision(), 100);
      BOOST_CHECK_EQUAL(atanh(f1).precision(), 100);
      BOOST_CHECK_EQUAL(log1p(f1).precision(), 100);
      BOOST_CHECK_EQUAL(expm1(f1).precision(), 100);
      BOOST_CHECK_EQUAL(cbrt(f1).precision(), 100);
   }

   return boost::report_errors();
}
