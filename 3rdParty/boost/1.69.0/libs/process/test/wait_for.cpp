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
#include <boost/process/error.hpp>
#include <boost/process/child.hpp>
#include <boost/process/args.hpp>
#include <thread>
#include <atomic>
#include <system_error>
#include <boost/asio.hpp>
#if defined(BOOST_POSIX_API)
#   include <signal.h>
#endif

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(wait_for)
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec
    );
    BOOST_REQUIRE(!ec);

    BOOST_CHECK(!c.wait_for(std::chrono::milliseconds(200)));
    BOOST_CHECK( c.wait_for(std::chrono::milliseconds(1000)));
}

BOOST_AUTO_TEST_CASE(wait_for_ec)
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec
    );
    BOOST_REQUIRE(!ec);

    BOOST_CHECK(!c.wait_for(std::chrono::milliseconds(400),ec));
    BOOST_CHECK( c.wait_for(std::chrono::milliseconds(1000),ec));

    BOOST_CHECK_MESSAGE(!ec, ec.message());
}


BOOST_AUTO_TEST_CASE(wait_until)
{
    using boost::unit_test::framework::master_test_suite;
    std::error_code ec;

    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec
    );
    BOOST_REQUIRE(!ec);

    auto now = std::chrono::system_clock::now();

    auto t1 = now + std::chrono::milliseconds(400);
    auto t2 = now + std::chrono::milliseconds(1200);

    BOOST_CHECK(!c.wait_until(t1));
    BOOST_CHECK( c.wait_until(t2));

}

BOOST_AUTO_TEST_CASE(wait_until_ec)
{
    using boost::unit_test::framework::master_test_suite;
    std::error_code ec;

    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec
    );
    BOOST_REQUIRE(!ec);

    auto now = std::chrono::system_clock::now();

    auto t1 = now + std::chrono::milliseconds(400);
    auto t2 = now + std::chrono::milliseconds(1200);

    BOOST_CHECK(!c.wait_until(t1, ec));
    BOOST_CHECK( c.wait_until(t2, ec));

    BOOST_CHECK_MESSAGE(!ec, ec.message());
}