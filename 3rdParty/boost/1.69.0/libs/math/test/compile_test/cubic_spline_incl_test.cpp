//  Copyright John Maddock 2006.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Basic sanity check that header <boost/math/special_functions/gamma.hpp>
// #includes all the files that it needs to.
//
#include <boost/math/interpolators/cubic_b_spline.hpp>
//
// Note this header includes no other headers, this is
// important if this test is to be meaningful:
//
#include "test_compile_result.hpp"

void compile_and_link_test()
{
   double data[] = { 1, 2, 3 };
   boost::math::cubic_b_spline<double> s(data, 3, 2, 1), s2;
   check_result<double>(s(1.0));
   check_result<double>(s.prime(1.0));
}
