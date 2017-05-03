//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_macro2
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_op_precedence )
{
  int a = 13, b = 2, c = 12;
  // left term of == is expanded in the logs
  BOOST_TEST(a % b == c);
  // right term of == is not expanded in the logs
  BOOST_TEST(a == c % b);
}

BOOST_AUTO_TEST_CASE( test_op_right_associative )
{
  int a = 1;
  BOOST_TEST(a);
  BOOST_TEST(!a);
  BOOST_TEST(--a);
}
//]
