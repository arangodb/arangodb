//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <boost/test/included/unit_test.hpp>
using namespace boost::unit_test;

void free_test_function()
{
  BOOST_TEST( 2 == 1 );
}

test_suite* init_unit_test_suite( int, char* [] )
{
  framework::master_test_suite().
    add( BOOST_TEST_CASE( &free_test_function ), 2 );

  return 0;
}
//]
