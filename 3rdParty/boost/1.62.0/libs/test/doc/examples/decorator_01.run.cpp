//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_01
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test_case1, * utf::label("trivial"))
{
  BOOST_TEST(true);
}

BOOST_AUTO_TEST_CASE(test_case2,
  * utf::label("trivial")
  * utf::label("cmp")
  * utf::description("testing equality of ones"))
{
  BOOST_TEST(1 == 1);
}
//]