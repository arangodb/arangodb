//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_message
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_message )
{
  const int a(1), b(2);
  BOOST_TEST(a == b, "a should be equal to b: " << a << "!=" << b);
  BOOST_TEST(a != 10, "value of a=" << a);
}
//]
