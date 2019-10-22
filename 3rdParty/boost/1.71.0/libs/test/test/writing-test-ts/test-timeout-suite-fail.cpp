//  (C) Copyright Raffi Enficiaud, 2019
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

//  See http://www.boost.org/libs/test for the library home page.

#define BOOST_TEST_MODULE timeout-error-suite
#include <boost/test/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <chrono>
#include <thread>

namespace utf = boost::unit_test;

// each of the test case of the data set has a timeout of 1 sec
// some of them will fail
BOOST_TEST_DECORATOR(* utf::timeout(1))
BOOST_DATA_TEST_CASE(test_success, utf::data::make({0,1,2,3}))
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100*5*sample));
    BOOST_TEST(sample >= 0);
}

// the full test suite has a timeout of 1s, some of the data test
// cases will not be executed
BOOST_TEST_DECORATOR(* utf::timeout(1))

BOOST_AUTO_TEST_SUITE(test_suite_timeout)
BOOST_DATA_TEST_CASE(test_success, utf::data::make({0,1,2,3,4,5,6,7,8}))
{
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    BOOST_TEST((sample >= 0 && sample < 5));
}


BOOST_AUTO_TEST_CASE(failure_should_not_be_executed)
{
    BOOST_TEST(false);
}

BOOST_AUTO_TEST_SUITE_END()
