//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#include <boost/test/included/unit_test.hpp>
#include <boost/test/parameterized_test.hpp>
using namespace boost::unit_test;

void free_test_function( int i )
{
  BOOST_TEST( i < 4 /* test assertion */ );
}

test_suite* init_unit_test_suite( int /*argc*/, char* /*argv*/[] )
{
  int params[] = { 1, 2, 3, 4, 5 };

  framework::master_test_suite().
    add( BOOST_PARAM_TEST_CASE( &free_test_function, params, params+5 ) );

  return 0;
}
//]
