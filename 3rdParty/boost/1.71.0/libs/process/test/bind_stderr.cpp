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
#include <system_error>

#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/process/args.hpp>
#include <boost/process/exe.hpp>
#include <boost/process/error.hpp>
#include <boost/process/io.hpp>
#include <boost/process/child.hpp>
#include <boost/process/async_pipe.hpp>

#include <boost/filesystem.hpp>

#include <string>
#include <istream>
#include <iostream>
#include <cstdlib>
#if defined(BOOST_WINDOWS_API)
#   include <windows.h>
typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
#   include <sys/wait.h>
#   include <unistd.h>
typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

namespace fs = boost::filesystem;
namespace bp = boost::process;
BOOST_AUTO_TEST_SUITE( bind_stderr );

BOOST_AUTO_TEST_CASE(sync_io, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;


    bp::ipstream is;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args={"test", "--echo-stderr", "hello"},
        bp::std_err>is,
        ec
    );
    BOOST_REQUIRE(!ec);

    std::string s;
    is >> s;
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

    bp::async_pipe p(io_context);
    std::error_code ec;
    bp::child c(
            bp::exe=master_test_suite().argv[1],
            bp::args+="test",
            bp::args+={"--echo-stderr", "abc"},
            bp::std_err>p,
            ec
        );
    BOOST_REQUIRE(!ec);


    boost::asio::streambuf buffer;
    boost::asio::async_read_until(p, buffer, '\n', read_handler(buffer));

    io_context.run();
}

BOOST_AUTO_TEST_CASE(nul, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;
    std::error_code ec;
    bp::child c(
        bp::exe(master_test_suite().argv[1]),
        bp::args+={"test", "--is-nul-stderr"},
        bp::std_err>bp::null,
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


BOOST_AUTO_TEST_CASE(file_io, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    fs::path pth =
            fs::path(master_test_suite().argv[1]).parent_path() / "std_err_log_file.txt";


    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args={"test", "--echo-stderr", "hello"},
        bp::std_err>pth,
        ec
    );
    BOOST_REQUIRE(!ec);

    c.wait();
    {
        fs::ifstream is{pth};

        std::string s;
        is >> s;
        BOOST_CHECK_EQUAL(s, "hello");
    }
    boost::filesystem::remove(pth);

}

BOOST_AUTO_TEST_SUITE_END();