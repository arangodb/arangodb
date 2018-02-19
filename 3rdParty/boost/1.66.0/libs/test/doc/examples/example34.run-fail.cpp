//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test )
{
  int i=2;
  BOOST_WARN( sizeof(int) == sizeof(short) );
  BOOST_CHECK( i == 1 );
  BOOST_REQUIRE( i > 5 );
  BOOST_CHECK( i == 6 );  // will never reach this check
}
//]
