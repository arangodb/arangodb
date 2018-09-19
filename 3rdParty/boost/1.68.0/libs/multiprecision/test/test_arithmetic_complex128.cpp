///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

#include <boost/config.hpp>
#ifdef BOOST_HAS_FLOAT128
#include <boost/multiprecision/complex128.hpp>
#endif

#include "libs/multiprecision/test/test_arithmetic.hpp"

int main()
{
#ifdef BOOST_HAS_FLOAT128
   test<boost::multiprecision::complex128>();
#endif
   return boost::report_errors();
}

