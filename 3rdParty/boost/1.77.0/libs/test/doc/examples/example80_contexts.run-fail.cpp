//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example80
#include <boost/test/included/unit_test.hpp>

void test()
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_CASE(test_case1)
{
  BOOST_TEST_INFO("Alpha");
  BOOST_TEST_INFO("Beta");
  BOOST_TEST(true);
  
  BOOST_TEST_INFO("Gamma");
  char a = 'a';
  BOOST_TEST_INFO("Delt" << a);
  test();
}
//]
