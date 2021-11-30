//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_07
#include <boost/test/included/unit_test.hpp>

namespace utf = boost::unit_test;

// test1 and test2 defined at the bottom

BOOST_AUTO_TEST_CASE(test3, * utf::depends_on("s1/test1"))
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_CASE(test4, * utf::depends_on("test3"))
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_CASE(test5, * utf::depends_on("s1/test2"))
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_SUITE(s1)

  BOOST_AUTO_TEST_CASE(test1)
  {
    BOOST_TEST(true);
  }

  BOOST_AUTO_TEST_CASE(test2, * utf::disabled())
  {
    BOOST_TEST(false);
  }

BOOST_AUTO_TEST_SUITE_END()
//]
