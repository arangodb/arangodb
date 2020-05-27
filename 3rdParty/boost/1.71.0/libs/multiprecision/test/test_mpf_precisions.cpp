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
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/detail/lightweight_test.hpp>
#include "test.hpp"

#include <boost/multiprecision/gmp.hpp>

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
   mpf_float::default_precision(100);
   mpf_float a("0.1");
   BOOST_CHECK_GE(a.precision(), 100);
   mpf_float::default_precision(20);
   {
      // test assignment from lvalue:
      mpf_float b(2);
      BOOST_CHECK_GE(b.precision(), 20);
      b = a;
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   {
      // test assignment from rvalue:
      mpf_float b(2);
      BOOST_CHECK_GE(b.precision(), 20);
      b = make_rvalue_copy(a);
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
#endif
   mpf_float::default_precision(20);
   {
      // test construct from lvalue:
      mpf_float b(a);
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
   {
      // test construct from rvalue:
      mpf_float b(make_rvalue_copy(a));
      BOOST_CHECK_EQUAL(b.precision(), a.precision());
   }
#endif
   {
      mpf_float f150(2, 150);
      BOOST_CHECK_GE(f150.precision(), 150);
   }
   {
      mpf_float f150("1.2", 150);
      BOOST_CHECK_GE(f150.precision(), 150);
   }
   //
   // From https://github.com/boostorg/multiprecision/issues/65
   //
   {
      mpf_float a(2);
      a.precision(100);
      BOOST_CHECK_EQUAL(a, 2);
      BOOST_CHECK_GE(a.precision(), 100);
   }


   //
   // string_view with explicit precision:
   //
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
   {
      std::string s("222");
      std::string_view v(s.c_str(), 1);
      mpf_float f(v, 100);
      BOOST_CHECK_EQUAL(f, 2);
      BOOST_CHECK_GE(f.precision(), 100);
   }
#endif
   // Swap:
   {
      mpf_float x(2, 100);  // 100 digits precision.
      mpf_float y(3, 50);   // 50 digits precision.
      swap(x, y);
      BOOST_CHECK_EQUAL(x, 3);
      BOOST_CHECK_EQUAL(y, 2);
      BOOST_CHECK_GE(x.precision(), 50);
      BOOST_CHECK_GE(y.precision(), 100);
      x.swap(y);
      BOOST_CHECK_EQUAL(x, 2);
      BOOST_CHECK_EQUAL(y, 3);
      BOOST_CHECK_GE(x.precision(), 100);
      BOOST_CHECK_GE(y.precision(), 50);
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
      x = std::move(mpf_float(y));
      BOOST_CHECK_EQUAL(x, y);
      BOOST_CHECK_EQUAL(x.precision(), y.precision());
#endif
   }
   {
      mpf_float c(4), d(8), e(9), f;
      f = (c + d) * d / e;
      mpf_float g((c + d) * d / e);
   }
   {
      mpf_float::default_precision(100);
      mpf_float f1;
      f1 = 3;
      BOOST_CHECK_GE(f1.precision(), 100);
      f1 = 3.5;
      BOOST_CHECK_GE(f1.precision(), 100);
      mpf_float f2(3.5);
      BOOST_CHECK_GE(f2.precision(), 100);
      mpf_float f3("5.1");
      BOOST_CHECK_GE(f3.precision(), 100);

      mpf_float::default_precision(50);
      mpf_float f4(f3, 50);
      BOOST_CHECK_GE(f4.precision(), 50);
      f4.assign(f1, f4.precision());
      BOOST_CHECK_GE(f4.precision(), 50);
   }


   return boost::report_errors();
}

