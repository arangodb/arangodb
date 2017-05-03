//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

struct F {
  F() : i( 0 ) { BOOST_TEST_MESSAGE( "setup fixture" ); }
  ~F()         { BOOST_TEST_MESSAGE( "teardown fixture" ); }

  int i;
};

BOOST_FIXTURE_TEST_CASE( test_case1, F )
{
  BOOST_TEST( i == 1 );
  ++i;
}

BOOST_FIXTURE_TEST_CASE( test_case2, F )
{
  BOOST_CHECK_EQUAL( i, 1 );
}

BOOST_AUTO_TEST_CASE( test_case3 )
{
  BOOST_TEST( true );
}
//]
