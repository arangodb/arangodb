//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_macro_workaround
#include <boost/test/included/unit_test.hpp>
#include <sstream>

BOOST_AUTO_TEST_CASE( test_logical_not_allowed )
{
  // Boost Unit Test Framework prevents compilation of
  // BOOST_TEST(true && true);
  BOOST_TEST((true && true)); // with extra brackets, it works as expected
}

BOOST_AUTO_TEST_CASE( test_ternary )
{
  int a = 1;
  int b = 2;
  // Boost Unit Test Framework prevents compilation of
  // BOOST_TEST(a == b ? true : false);
  BOOST_TEST((a + 1 == b ? true : false)); // again works as expected with extra brackets
}


//]
