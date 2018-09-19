//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_06
#include <boost/test/included/unit_test.hpp>
namespace utf = boost::unit_test;

const bool io_implemented = true;
const bool db_implemented = false;

BOOST_AUTO_TEST_CASE(test_io,
  * utf::enable_if<io_implemented>())
{
  BOOST_TEST(1 != 1);
}

BOOST_AUTO_TEST_CASE(test_db,
  * utf::enable_if<db_implemented>())
{
  BOOST_TEST(2 != 2);
}
//]
