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
#include <boost/process/async.hpp>
#include <boost/process/io.hpp>
#include <boost/process/spawn.hpp>
#include <boost/process/child.hpp>

#include <boost/thread.hpp>
#include <future>

#include <boost/system/error_code.hpp>

#include <boost/algorithm/string/predicate.hpp>

BOOST_AUTO_TEST_SUITE( async );


using namespace std;

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(async_out_future, *boost::unit_test::timeout(2))
{

    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context io_context;


    std::error_code ec;

    std::future<std::string> fut;
    std::future<void> fut_in;
    boost::asio::streambuf in_buf;


    std::ostream ostr(&in_buf);
    ostr << "-string" << endl ;

    bp::spawn(
        master_test_suite().argv[1],
        "test", "--prefix-once", "test",
        bp::std_in  < in_buf > fut_in,
        bp::std_out > fut,
        io_context,
        ec
    );
    BOOST_REQUIRE(!ec);


    io_context.run();

    BOOST_REQUIRE(fut.valid());
    BOOST_REQUIRE(fut_in.valid());
    BOOST_CHECK_NO_THROW(fut_in.get());

    std::string line;

    BOOST_CHECK_NO_THROW(line = fut.get());

    std::string val = "test-string";
    BOOST_REQUIRE_GE(line.size(), val.size());
    if (line >= val)
        BOOST_CHECK(boost::algorithm::starts_with(line, val));
}


BOOST_AUTO_TEST_CASE(emtpy_out, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context io_context;


    std::error_code ec;
    std::future<std::string> fut;

    bp::spawn(
        master_test_suite().argv[1],
        "test", "--exit-code", "0",
        bp::std_out > fut,
        io_context,
        ec
    );
    BOOST_REQUIRE(!ec);


    io_context.run();

    BOOST_REQUIRE(fut.valid());
    BOOST_CHECK_EQUAL(fut.get(), "");
}

BOOST_AUTO_TEST_SUITE_END();