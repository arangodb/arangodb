//  (C) Copyright Raffi Enficiaud 2018.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE basic_smoke_test3
#include <boost/test/included/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(some_suite)

BOOST_AUTO_TEST_CASE(case1)
{
  BOOST_TEST(false);
}

BOOST_AUTO_TEST_SUITE_END();

BOOST_AUTO_TEST_CASE(case2)
{
  BOOST_TEST(true);
}
