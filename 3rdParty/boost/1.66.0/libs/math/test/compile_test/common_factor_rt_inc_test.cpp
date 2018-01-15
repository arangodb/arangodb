//  Copyright John Maddock 2017.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Basic sanity check that header <boost/math/tools/config.hpp>
// #includes all the files that it needs to.
//
#include <boost/math/common_factor_rt.hpp>
//
// Note this header includes no other headers, this is
// important if this test is to be meaningful:
//
#include "test_compile_result.hpp"

inline void check_result_imp(std::pair<unsigned long, unsigned long*>, std::pair<unsigned long, unsigned long*>) {}


void compile_and_link_test()
{
   int i(3), j(4);
   check_result<int>(boost::math::gcd(i, j));
   check_result<int>(boost::math::lcm(i, j));

   unsigned long arr[] = { 45, 56, 99, 101 };
   check_result<std::pair<unsigned long, unsigned long*> >(boost::math::gcd_range(&arr[0], &arr[0] + 4));

}
