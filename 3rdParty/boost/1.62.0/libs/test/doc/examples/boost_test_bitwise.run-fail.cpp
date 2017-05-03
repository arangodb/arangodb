//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_bitwise
#include <boost/test/included/unit_test.hpp>
#include <sstream>

BOOST_AUTO_TEST_CASE(test_bitwise)
{
  namespace tt = boost::test_tools;
  int a = 0xAB;
  BOOST_TEST( a == (a & ~1), tt::bitwise() );
  BOOST_TEST( a == a + 1, tt::bitwise() );
  BOOST_TEST( a != a + 1, tt::bitwise() );
  int b = 0x88;
  BOOST_TEST( a == b, tt::bitwise() );
}
//]
