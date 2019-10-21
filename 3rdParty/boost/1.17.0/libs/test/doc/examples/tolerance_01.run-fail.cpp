//  Copyright (c) 2015 Boost.Test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE tolerance_01
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test1, * utf::tolerance(0.00001))
{
  double x = 10.0000000;
  double y = 10.0000001;
  double z = 10.001;
  BOOST_TEST(x == y); // irrelevant difference
  BOOST_TEST(x == z); // relevant difference
}
//]
