// Copyright (C) 2007 Anthony Williams
//
//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MODULE Boost.Threads: hardware_concurrency test suite

#include <boost/thread/thread_only.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/thread/mutex.hpp>

BOOST_AUTO_TEST_CASE(test_hardware_concurrency_is_non_zero)
{
    BOOST_CHECK(boost::thread::hardware_concurrency()!=0);
}


