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
#include <boost/process/io.hpp>
#include <boost/process/child.hpp>
#include <boost/process/exe.hpp>
#include <boost/process/args.hpp>
#include <boost/process/async_pipe.hpp>

#include <system_error>

#include <boost/system/error_code.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <string>
#include <istream>
#if defined(BOOST_WINDOWS_API)
#   include <windows.h>
typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(sync_io, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::pipe p1;
    bp::pipe p2;

    bp::ipstream is1;
    bp::ipstream is2;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--echo-stdout-stderr", "hello",
        bp::std_out>is1,
        bp::std_err>is2,
        ec
    );
    BOOST_REQUIRE(!ec);



    std::string s;
    is1 >> s;
    BOOST_CHECK_EQUAL(s, "hello");

    is2 >> s;
    BOOST_CHECK_EQUAL(s, "hello");
}


struct read_handler
{
    boost::asio::streambuf &buffer_;

    read_handler(boost::asio::streambuf &buffer) : buffer_(buffer) {}

    void operator()(const boost::system::error_code &ec, std::size_t size)
    {
        BOOST_REQUIRE(!ec);
        std::istream is(&buffer_);
        std::string line;
        std::getline(is, line);
        BOOST_CHECK(boost::algorithm::starts_with(line, "abc"));
    }
};

BOOST_AUTO_TEST_CASE(async_io, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;
    boost::asio::io_context io_context;

    bp::async_pipe p1(io_context);
    bp::async_pipe p2(io_context);

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args={"test", "--echo-stdout-stderr", "abc"},
        bp::std_out > p1,
        bp::std_err > p2,
        ec
    );
    BOOST_REQUIRE(!ec);

    boost::asio::streambuf buffer1;
    boost::asio::async_read_until(p1, buffer1, '\n',
        read_handler(buffer1));

    boost::asio::streambuf buffer2;
    boost::asio::async_read_until(p2, buffer2, '\n',
        read_handler(buffer2));

    io_context.run();
}

BOOST_AUTO_TEST_CASE(nul, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;
    std::error_code ec;
    bp::child c(
        bp::exe(master_test_suite().argv[1]),
        bp::args+={"test", "--echo-stdout-stderr", "some string"},
        (bp::std_err & bp::std_out) > bp::null,
        ec
    );
    BOOST_REQUIRE(!ec);

    c.wait();
    int exit_code = c.exit_code();
#if defined(BOOST_WINDOWS_API)
    BOOST_CHECK_EQUAL(EXIT_SUCCESS, exit_code);
#elif defined(BOOST_POSIX_API)
    BOOST_CHECK_EQUAL(EXIT_SUCCESS, exit_code);
#endif
}
