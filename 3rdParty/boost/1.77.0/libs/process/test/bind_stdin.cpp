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
#include <boost/process/args.hpp>
#include <boost/process/async.hpp>

#include <system_error>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>

#include <boost/asio.hpp>
#include <string>
#include <cstdlib>
#include <iostream>
#include <thread>

#include <boost/config.hpp>

BOOST_AUTO_TEST_SUITE( bind_stdin );

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

BOOST_AUTO_TEST_CASE(sync_io, *boost::unit_test::timeout(10))
{
    std::cout << "sync_io" << std::endl;
    using boost::unit_test::framework::master_test_suite;

    bp::opstream os;
    bp::ipstream is;

    std::error_code ec;
  
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--prefix", "abc"},
        bp::std_in <os,
        bp::std_out>is,
        ec);
    
    BOOST_REQUIRE(!ec);
  
  

    os << "hello" << std::endl;
      
    std::string s;


    is >> s;
    BOOST_CHECK_EQUAL(s, "abchello");
    os << 123 << std::endl;
    is >> s;
    BOOST_CHECK_EQUAL(s, "abc123");
    os << 3.1415 << std::endl;
    is >> s;
    BOOST_CHECK_EQUAL(s, "abc3.1415");

    c.terminate();
    c.wait();


    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    int i = -1;
    is >> i;
    BOOST_CHECK(is.eof());
    BOOST_CHECK(!is);
}


struct write_handler
{
    bp::ipstream &is_;

    write_handler(bp::ipstream &is) : is_(is) {}

    void operator()(const boost::system::error_code &ec, std::size_t size)
    {
        BOOST_REQUIRE_EQUAL(6u, size);
        std::string s;
        is_ >> s;
        BOOST_CHECK_EQUAL(s, "abchello");
    }
};

BOOST_AUTO_TEST_CASE(async_io, *boost::unit_test::timeout(2))
{
    std::cout << "async_io" << std::endl;
    using boost::unit_test::framework::master_test_suite;

    boost::asio::io_context io_context;

    bp::async_pipe p1(io_context);
    bp::ipstream is;

    boost::asio::streambuf sb;
    std::ostream os(&sb);

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--prefix-once", "abc",
        bp::std_in<p1,
        bp::std_out>is,
        ec
    );
    BOOST_REQUIRE(!ec);

    os << "hello" << std::endl;

  //  std::string s = "hello\n";
    boost::asio::async_write(p1, sb,
        write_handler(is));

    io_context.run();

    c.wait();
}

BOOST_AUTO_TEST_CASE(nul, *boost::unit_test::timeout(2))
{
    std::cout << "nul" << std::endl;

    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--is-nul-stdin",
        bp::std_in<bp::null,
        ec);

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
    std::cout << "file_io" << std::endl;
    using boost::unit_test::framework::master_test_suite;


    fs::path pth =
            fs::path(master_test_suite().argv[1]).parent_path() / "std_in_log_file.txt";
    bp::ipstream is;

    {
        boost::filesystem::ofstream fs(pth);
        fs << 321 << std::endl;
        fs << 1.2345 << std::endl;
        fs << "some_string" << std::endl;
    }
    std::error_code ec;

    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--prefix", "abc"},
        bp::std_in <pth,
        bp::std_out>is,
        ec);

    BOOST_REQUIRE(!ec);


    std::string s;


    is >> s;
    BOOST_CHECK_EQUAL(s, "abc321");
    is >> s;
    BOOST_CHECK_EQUAL(s, "abc1.2345");
    is >> s;
    BOOST_CHECK_EQUAL(s, "abcsome_string");

    c.wait();
    boost::filesystem::remove(pth);
}

BOOST_AUTO_TEST_CASE(file_io_C, *boost::unit_test::timeout(2))
{
    //tested, since stdin also yields FILE*.
    std::cout << "file_io_C" << std::endl;
    using boost::unit_test::framework::master_test_suite;


    fs::path pth =
            fs::path(master_test_suite().argv[1]).parent_path() / "std_in_log_file_2.txt";
    bp::ipstream is;

    {
        boost::filesystem::ofstream fs(pth);
        fs << 321 << std::endl;
        fs << 1.2345 << std::endl;
        fs << "some_string" << std::endl;
    }

    FILE * f = fopen(pth.string().c_str(), "r+");

    BOOST_REQUIRE(f != nullptr);
    std::error_code ec;

    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"test", "--prefix", "abc"},
        bp::std_in <f,
        bp::std_out>is,
        ec);

    fclose(f);

    BOOST_REQUIRE(!ec);


    std::string s;


    is >> s;
    BOOST_CHECK_EQUAL(s, "abc321");
    is >> s;
    BOOST_CHECK_EQUAL(s, "abc1.2345");
    is >> s;
    BOOST_CHECK_EQUAL(s, "abcsome_string");

    c.wait();
    boost::filesystem::remove(pth);
}

BOOST_AUTO_TEST_SUITE_END();