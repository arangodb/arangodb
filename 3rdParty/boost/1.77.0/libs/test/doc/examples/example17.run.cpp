//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( my_test1, 1 )

BOOST_AUTO_TEST_CASE( my_test1 )
{
  BOOST_TEST( 2 == 1 );
}

BOOST_AUTO_TEST_SUITE( internal )

BOOST_AUTO_TEST_CASE_EXPECTED_FAILURES( my_test1, 2 )

BOOST_AUTO_TEST_CASE( my_test1 )
{
  BOOST_CHECK_EQUAL( sizeof(int), sizeof(char) );
  BOOST_CHECK_EQUAL( sizeof(int*), sizeof(char) );
}

BOOST_AUTO_TEST_SUITE_END()
//]
