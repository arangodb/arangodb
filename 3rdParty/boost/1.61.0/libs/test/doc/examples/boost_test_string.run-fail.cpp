//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_strings
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_pointers )
{
  float a(0.5f), b(0.5f);
  const float* pa = &a, *pb = &b;
  BOOST_TEST(a == b);
  BOOST_TEST(pa == pb);
}

BOOST_AUTO_TEST_CASE( test_strings )
{
  const char* a = "test1";
  const char* b = "test2";
  const char* c = "test1";
  BOOST_TEST(a == b);
  BOOST_TEST(a == c);
}
//]
