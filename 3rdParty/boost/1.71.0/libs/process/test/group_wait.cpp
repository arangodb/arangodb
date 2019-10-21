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
#include <fstream>
#include <boost/system/error_code.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/process/error.hpp>
#include <boost/process/io.hpp>
#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <boost/process/group.hpp>
#include <system_error>

#include <string>
#include <thread>
#include <istream>
#include <iostream>
#include <cstdlib>

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(wait_group_test, *boost::unit_test::timeout(5))
{
    std::atomic<bool> done{false};
    std::thread thr{
        [&]
        {
            for (int i = 0; i < 50 && !done.load(); i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            BOOST_REQUIRE(done.load());
        }};


    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::group g;


    bp::child c1(
            master_test_suite().argv[1],
            "--wait", "2",
            g,
            ec
    );

    bp::child c2(
            master_test_suite().argv[1],
            "--wait", "2",
            g,
            ec
    );

    BOOST_CHECK(c1.running());
    BOOST_CHECK(c2.running());

    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(c1.in_group(ec));
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    BOOST_REQUIRE(c2.in_group(ec));
    BOOST_CHECK_MESSAGE(!ec, ec.message());
    g.wait();

    BOOST_CHECK(!c1.running());
    BOOST_CHECK(!c2.running());

    done.store(true);
    thr.join();
}


BOOST_AUTO_TEST_CASE(wait_group_test_timeout, *boost::unit_test::timeout(15))
{
    using boost::unit_test::framework::master_test_suite;

    std::atomic<bool> done{false};
    std::thread thr{
            [&]
            {
                for (int i = 0; i < 150 && !done.load(); i++)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                BOOST_REQUIRE(done.load());
            }};

    std::error_code ec;
    bp::group g;


    bp::child c1(
            master_test_suite().argv[1],
            "--wait", "1",
            g,
            ec
    );

    bp::child c2(
            master_test_suite().argv[1],
            "--wait", "4",
            g,
            ec
    );

    BOOST_CHECK(c1.running());
    BOOST_CHECK(c2.running());

    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(c1.in_group());
    BOOST_REQUIRE(c2.in_group());

    BOOST_CHECK(!g.wait_for(std::chrono::seconds(2), ec));

    BOOST_CHECK_MESSAGE(!ec, std::to_string(ec.value()) + " == " + ec.message());
    BOOST_CHECK(!c1.running());
    BOOST_CHECK(c2.running());

    BOOST_CHECK(g.wait_for(std::chrono::seconds(5), ec));
    BOOST_CHECK_MESSAGE(!ec, std::to_string(ec.value()) + " == " + ec.message());
    BOOST_CHECK(!c1.running());
    BOOST_CHECK(!c2.running());

    done.store(true);
    thr.join();
}