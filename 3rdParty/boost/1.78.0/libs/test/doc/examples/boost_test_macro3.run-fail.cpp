//  (C) Copyright Raffi Enficiaud 2014.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE boost_test_macro3
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_CASE( test_op_reportings )
{
  int a = 13, b = 12;
  BOOST_TEST(a == b);
  BOOST_TEST(a < b);
  BOOST_TEST(a - 1 < b);  
  BOOST_TEST(b > a - 1);    
}
//]
