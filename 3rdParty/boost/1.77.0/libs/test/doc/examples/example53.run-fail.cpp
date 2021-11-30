//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE( test_suite )

BOOST_AUTO_TEST_CASE( test_case1 )
{
  BOOST_ERROR( "some error 1" );
}

BOOST_AUTO_TEST_SUITE_END()


BOOST_AUTO_TEST_CASE( test_case_on_file_scope )
{
  BOOST_TEST( true );
}

BOOST_AUTO_TEST_SUITE( test_suite )

BOOST_AUTO_TEST_CASE( test_case2 )
{
  BOOST_ERROR( "some error 2" );
}

BOOST_AUTO_TEST_SUITE_END()
//]
