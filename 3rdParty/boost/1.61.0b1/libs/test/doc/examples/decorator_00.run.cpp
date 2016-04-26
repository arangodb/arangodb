//  (C) Copyright Andrzej Krzemienski 2015.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

//[example_code
#define BOOST_TEST_MODULE decorator_00
#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>

namespace utf  = boost::unit_test;
namespace data = boost::unit_test::data;

BOOST_TEST_DECORATOR(* utf::description("with description"))
BOOST_DATA_TEST_CASE(test_1, data::xrange(4))
{
    BOOST_TEST(sample >= 0);
}
//]