// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN

#include <boost/test/included/unit_test.hpp>
#include <boost/process/error.hpp>
#include <boost/process/child.hpp>
#include <boost/process/args.hpp>
#include <boost/process/async.hpp>
#include <thread>
#include <atomic>
#include <system_error>
#include <boost/asio.hpp>
#if defined(BOOST_POSIX_API)
#   include <signal.h>
#endif

namespace bp = boost::process;

BOOST_AUTO_TEST_SUITE( wait_test );


BOOST_AUTO_TEST_CASE(sync_wait, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec
    );
    BOOST_REQUIRE(!ec);

    c.wait();

}

BOOST_AUTO_TEST_CASE(async_wait, *boost::unit_test::timeout(4))
{
    using boost::unit_test::framework::master_test_suite;
    using namespace boost::asio;

    boost::asio::io_context io_context;

    std::error_code ec;
    bool called = false;

    boost::asio::deadline_timer timeout{io_context, boost::posix_time::seconds(3)};
    timeout.async_wait([&](boost::system::error_code ec){if (!ec) io_context.stop();});


    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--wait", "1"},
        ec,
        io_context,
        bp::on_exit([&](int, const std::error_code&){called = true;})

    );
    BOOST_REQUIRE(!ec);

    io_context.run();
    BOOST_CHECK(called);

}

BOOST_AUTO_TEST_SUITE_END();
