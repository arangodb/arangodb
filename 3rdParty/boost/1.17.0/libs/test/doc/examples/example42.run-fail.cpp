//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.


//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>
#include <boost/test/tools/floating_point_comparison.hpp>

BOOST_AUTO_TEST_CASE( test )
{
  double v1 = 1.23456e-10;
  double v2 = 1.23457e-10;

  BOOST_CHECK_CLOSE( v1, v2, 0.0001 );
  // Absolute value of difference between these two values is 1e-15. They seems
  // to be very close. But we want to checks that these values differ no more then 0.0001%
  // of their value. And this test will fail at tolerance supplied.
}
//]
