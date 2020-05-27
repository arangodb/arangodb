///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#define NO_MIXED_OPS

#include <boost/multiprecision/cpp_complex.hpp>

#include "libs/multiprecision/test/test_arithmetic.hpp"

int main()
{
   test<boost::multiprecision::cpp_complex_50>();
   test<boost::multiprecision::number<boost::multiprecision::complex_adaptor<boost::multiprecision::cpp_bin_float<50> >, boost::multiprecision::et_on> >();
   return boost::report_errors();
}

