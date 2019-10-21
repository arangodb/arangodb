//  (C) Copyright Raffi Enficiaud 2019.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE decorators in dataset

#include <boost/test/unit_test.hpp>
#include <boost/test/framework.hpp>
#include <boost/test/data/test_case.hpp>
#include <chrono>
#include <thread>

using namespace boost::unit_test;

BOOST_TEST_DECORATOR(*decorator::expected_failures(1))
BOOST_TEST_DECORATOR(*decorator::description("checking the proper description at the test case"))
BOOST_DATA_TEST_CASE(test_expected_failure, data::make({1,2,3,4,5}), value)
{
    BOOST_TEST(false);
}

BOOST_TEST_DECORATOR(*decorator::timeout(1))
BOOST_DATA_TEST_CASE(test_timeout, data::xrange(2), value)
{
    // using namespace std::chrono_literals; // C++14
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    BOOST_TEST(true);
}
