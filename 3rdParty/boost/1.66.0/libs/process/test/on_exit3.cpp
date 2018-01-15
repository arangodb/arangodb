// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#define BOOST_TEST_IGNORE_SIGCHLD
#include <boost/test/included/unit_test.hpp>
#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <thread>

BOOST_AUTO_TEST_CASE(double_ios_threaded, *boost::unit_test::timeout(6))
{
    using boost::unit_test::framework::master_test_suite;

    if (master_test_suite().argc > 2 && strcmp(master_test_suite().argv[1], "sleep") == 0)
    {
        auto s = atoi(master_test_suite().argv[2]);
        std::this_thread::sleep_for(std::chrono::seconds(s));
        return;
    }

    namespace bp = boost::process;
    boost::asio::io_context ios;
    std::chrono::steady_clock::time_point p1, p2;

    // launch a child that will sleep for 2s
    auto c1 = bp::child(master_test_suite().argv[0], "sleep", "2", ios,
        bp::on_exit([&p1](int, const std::error_code&)
    { p1 = std::chrono::steady_clock::now(); }));

    // wait a bit, make sure the child launch for my test
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    // launch a child that will sleep for 4s
    auto c2 = bp::child(master_test_suite().argv[0], "sleep", "4", ios,
        bp::on_exit([&p2](int, const std::error_code&)
    { p2 = std::chrono::steady_clock::now(); }));

    // wait for the notifications
    std::thread ([&ios] { ios.run(); }).join();

    BOOST_REQUIRE((p2 - p1) > std::chrono::seconds(1));
}

