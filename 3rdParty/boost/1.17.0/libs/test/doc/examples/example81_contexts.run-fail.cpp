//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE example81
#include <boost/test/included/unit_test.hpp>

void test()
{
  BOOST_TEST(2 != 2);
}

BOOST_AUTO_TEST_CASE(test_case1)
{
  BOOST_TEST_CONTEXT("Alpha") {
    BOOST_TEST(1 != 1);
    test();
    
    BOOST_TEST_CONTEXT("Be" << "ta")
      BOOST_TEST(3 != 3);
      
    BOOST_TEST(4 == 4);
  }
  
  BOOST_TEST(5 != 5);
}
//]
