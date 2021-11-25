//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

#include <cmath>

BOOST_AUTO_TEST_CASE( test )
{
  // sin 45 radians is actually ~ 0.85, sin 45 degrees is ~0.707
  double res = std::sin( 45. ); 

  BOOST_WARN_MESSAGE( res < 0.71, 
                      "sin(45){" << res << "} is > 0.71. Arg is not in radian?" );
}
//]
