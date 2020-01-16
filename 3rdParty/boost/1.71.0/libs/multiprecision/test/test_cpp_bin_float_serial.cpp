///////////////////////////////////////////////////////////////
//  Copyright 2013 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

//
// Compare arithmetic results using fixed_int to GMP results.
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_bin_float.hpp>
#include "test_float_serial.hpp"

#if !defined(TEST1) && !defined(TEST2)
#  define TEST1
#  define TEST2
#endif


int main()
{
   using namespace boost::multiprecision;
#ifdef TEST1
   test<cpp_bin_float_50>();
#endif
#ifdef TEST2
   test<cpp_bin_float_quad>();
#endif
   return boost::report_errors();
}



