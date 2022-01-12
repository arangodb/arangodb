//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_macro_overview
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_macro_overview )
{
  namespace tt = boost::test_tools;
  int a = 1;
  int b = 2;
  BOOST_TEST(a != b - 1);
  BOOST_TEST(a + 1 < b);
  BOOST_TEST(b -1 > a, a << " < " << b - 1 << " does not hold");
  BOOST_TEST(a == b, tt::bitwise());
  BOOST_TEST(a + 0.1 == b - 0.8, tt::tolerance(0.01));
}
//]
