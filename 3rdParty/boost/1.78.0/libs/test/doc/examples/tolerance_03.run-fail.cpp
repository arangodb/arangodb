//  Copyright (c) 2015 Boost.Test team
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE tolerance_03
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

double x = 10.000000;
double d =  0.000001;

BOOST_AUTO_TEST_CASE(passing, * utf::tolerance(0.0001))
{
  BOOST_TEST(x == x + d); // equal with tolerance
  BOOST_TEST(x >= x + d); // ==> greater-or-equal

  BOOST_TEST(d == .0);    // small with tolerance
}

BOOST_AUTO_TEST_CASE(failing, * utf::tolerance(0.0001))
{
  BOOST_TEST(x - d <  x); // less, but still too close
  BOOST_TEST(x - d != x); // unequal but too close

  BOOST_TEST(d > .0);     // positive, but too small
  BOOST_TEST(d < .0);     // not sufficiently negative
}
//]
