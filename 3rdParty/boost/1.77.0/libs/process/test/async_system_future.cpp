// Copyright (c) 2016 Klemens D. Morgenstern
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>

#include <boost/process/error.hpp>
#include <boost/process/io.hpp>
#include <boost/process/async.hpp>
#include <boost/process/child.hpp>
#include <boost/process/async_system.hpp>

#include <string>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/yield.hpp>

#include <vector>
#include <array>

namespace bp = boost::process;
BOOST_AUTO_TEST_SUITE( async );

BOOST_AUTO_TEST_CASE(future, *boost::unit_test::timeout(15))
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context ios;

    std::future<int> fut = bp::async_system(
                              ios, boost::asio::use_future,
                              master_test_suite().argv[1],
                              "test", "--exit-code", "42");

    ios.run();

    int exit_code = 0;
    BOOST_CHECK_NO_THROW(exit_code = fut.get());

    BOOST_CHECK_EQUAL(exit_code, 42);
}

BOOST_AUTO_TEST_CASE(future_error, *boost::unit_test::timeout(15))
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context ios;

    std::future<int> fut = bp::async_system(
                              ios, boost::asio::use_future,
                              "invalid-command");

    ios.run();

    BOOST_CHECK_THROW(fut.get(), boost::system::system_error);
}

BOOST_AUTO_TEST_SUITE_END();
