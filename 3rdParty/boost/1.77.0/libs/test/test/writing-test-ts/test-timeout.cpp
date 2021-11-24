//  (C) Copyright Raffi Enficiaud, 2019
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE timeout-error
#include <boost/test/unit_test.hpp>
#include <chrono>
#include <thread>

namespace utf = boost::unit_test;

BOOST_AUTO_TEST_CASE(test1, * utf::timeout(1))
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    BOOST_TEST(true);
}

BOOST_AUTO_TEST_CASE(test2, * utf::timeout(3))
{
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    BOOST_TEST(true);
}
