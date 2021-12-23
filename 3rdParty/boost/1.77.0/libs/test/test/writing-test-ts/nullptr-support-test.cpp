//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE nullptr_support
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(test1)
{
    int *p = nullptr;
    BOOST_TEST(p == nullptr);
}

BOOST_AUTO_TEST_CASE(test2)
{
    int *p = nullptr;
    BOOST_CHECK_EQUAL(p, nullptr);
}
