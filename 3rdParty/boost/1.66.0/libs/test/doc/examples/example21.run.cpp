//  (C) Copyright Gennadiy Rozental 2011-2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_init )
{
  int current_time = 0; // real call is required here

  BOOST_TEST_MESSAGE( "Testing initialization :" );
  BOOST_TEST_MESSAGE( "Current time:" << current_time );
}

BOOST_AUTO_TEST_CASE( test_update )
{
  std::string field_name = "Volume";
  int         value      = 100;

  BOOST_TEST_MESSAGE( "Testing update :" );
  BOOST_TEST_MESSAGE( "Update " << field_name << " with " << value );
}
//]
