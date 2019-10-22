///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/tommath.hpp>

#include "test_arithmetic.hpp"

template <>
struct is_twos_complement_integer<boost::multiprecision::tom_int> : public boost::mpl::false_ {};

int main()
{
   test<boost::multiprecision::number<boost::multiprecision::rational_adaptor<boost::multiprecision::tommath_int> > >();
   return boost::report_errors();
}

