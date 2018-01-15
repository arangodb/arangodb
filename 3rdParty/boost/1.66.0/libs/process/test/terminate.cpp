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
#include <boost/process/exe.hpp>
#include <boost/process/child.hpp>
#include <boost/process/args.hpp>
#include <system_error>
#include <thread>

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(terminate_set_on_error)
{
    using boost::unit_test::framework::master_test_suite;
    std::error_code ec;
    bp::child c(
        bp::exe(master_test_suite().argv[1]),
        bp::args+={"test", "--loop"},
        ec
    );
    BOOST_REQUIRE(!ec);

    BOOST_CHECK(c.valid());
    BOOST_CHECK(c.running(ec));

    BOOST_CHECK(!c.wait_for(std::chrono::milliseconds(100), ec));
    BOOST_CHECK(c.running(ec));
    BOOST_CHECK(c.valid());

    c.terminate(ec);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    BOOST_CHECK(!c.running(ec));
    BOOST_CHECK(!ec);
}

BOOST_AUTO_TEST_CASE(terminate_throw_on_error)
{
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+="test",
        bp::args+="--loop",
        ec
    );
    BOOST_REQUIRE(!ec);
    BOOST_CHECK(c.valid());
    BOOST_CHECK(c.running());

    BOOST_CHECK(!c.wait_for(std::chrono::milliseconds(100), ec));
    BOOST_CHECK(c.running(ec));
    BOOST_CHECK(c.valid());

    c.terminate();
    std::this_thread::sleep_for(std::chrono::milliseconds(5)); 
    BOOST_CHECK(!c.running());
}
