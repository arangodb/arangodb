//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_10
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_SUITE(suite1,
  * utf::expected_failures(1))

  BOOST_AUTO_TEST_CASE(test1,
    * utf::expected_failures(2))
  {
    BOOST_TEST(false);
    BOOST_TEST(false);
  }
  
  BOOST_AUTO_TEST_CASE(test2)
  {
    BOOST_TEST(false);
    BOOST_TEST(false);
  }

BOOST_AUTO_TEST_SUITE_END()
//]
