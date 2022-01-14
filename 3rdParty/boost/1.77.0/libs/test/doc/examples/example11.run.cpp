//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

void test_case1() { /* ... */ }
void test_case2() { /* ... */ }
void test_case3() { /* ... */ }
void test_case4() { /* ... */ }

test_suite* init_unit_test_suite( int /*argc*/, char* /*argv*/[] )
{
  test_suite* ts1 = BOOST_TEST_SUITE( "test_suite1" );
  ts1->add( BOOST_TEST_CASE( &test_case1 ) );
  ts1->add( BOOST_TEST_CASE( &test_case2 ) );

  test_suite* ts2 = BOOST_TEST_SUITE( "test_suite2" );
  ts2->add( BOOST_TEST_CASE( &test_case3 ) );
  ts2->add( BOOST_TEST_CASE( &test_case4 ) );

  framework::master_test_suite().add( ts1 );
  framework::master_test_suite().add( ts2 );

  return 0;
}
//]
