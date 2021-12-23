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

BOOST_AUTO_TEST_CASE(group_test, *boost::unit_test::timeout(5))
{
    std::cout << "group_test" << std::endl;
    using boost::unit_test::framework::master_test_suite;

    std::error_code ec;
    bp::group g;


    bp::child c(
        master_test_suite().argv[1],
        g,
        ec
    );
    BOOST_CHECK(c.running());

    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(c.in_group());
    BOOST_CHECK(c);
    BOOST_CHECK(c.running());

    BOOST_REQUIRE_NO_THROW(g.terminate()); 
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    BOOST_CHECK(!c.running());
    if (c.running())
        c.terminate();

    std::cout << "group_test out" << std::endl;

}

BOOST_AUTO_TEST_CASE(attached, *boost::unit_test::timeout(5))
{
    std::cout << "attached" << std::endl;

    using boost::unit_test::framework::master_test_suite;

    bp::ipstream is;

    bp::group g;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"--launch-attached"},
        bp::std_out>is,
        g,
        ec
    );
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(c.in_group(ec));
    BOOST_CHECK(c);



    bp::pid_t pid;
    is >> pid;
    bp::child sub_c(pid);
    is >> pid; //invalid pid.


    BOOST_REQUIRE(sub_c);
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); //just to be sure.
    

#if defined( BOOST_POSIX_API )
    ::waitpid(sub_c.id(), nullptr, WNOHANG);
    BOOST_CHECK(kill(sub_c.id(), 0) == 0);
#else
    BOOST_CHECK(sub_c.running());
#endif
    
    BOOST_REQUIRE_NO_THROW(g.terminate()); 
    
    BOOST_CHECK(sub_c);
    std::this_thread::sleep_for(std::chrono::milliseconds(100)); //just to be sure.

    BOOST_CHECK(!c.running());

#if defined( BOOST_POSIX_API )
    errno = 0;
    ::waitpid(sub_c.id(), nullptr, WNOHANG);
    bool still_runs = (kill(sub_c.id(), 0) == 0) && (errno != ECHILD) && (errno != ESRCH);
#else
    bool still_runs = sub_c.running();
#endif
    BOOST_CHECK_MESSAGE(!still_runs, boost::process::detail::get_last_error().message());

    if (still_runs)
        sub_c.terminate();
    BOOST_CHECK(!c.running());
    if (c.running())
        c.terminate();

    std::cout << "attached out" << std::endl;

}



BOOST_AUTO_TEST_CASE(detached, *boost::unit_test::timeout(5))
{
    std::cerr << "detached" << std::endl;

    using boost::unit_test::framework::master_test_suite;

    bp::ipstream is;

    bp::group g;


    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        bp::args+={"--launch-detached"},
        bp::std_out>is,
        g,
        ec
    );

    BOOST_REQUIRE(!ec);
    BOOST_CHECK(c);

    bp::pid_t pid;
    is >> pid;
    is >> pid;
    bp::child sub_c(pid);

    std::this_thread::sleep_for(std::chrono::milliseconds(50)); //just to be sure.

#if defined( BOOST_POSIX_API )
    BOOST_CHECK(kill(sub_c.id(), 0) == 0);
#else
    BOOST_CHECK(sub_c.running());
#endif

    BOOST_REQUIRE_NO_THROW(g.terminate()); 

    BOOST_CHECK(sub_c);
    std::this_thread::sleep_for(std::chrono::milliseconds(50)); //just to be sure.

#if defined( BOOST_POSIX_API )
    bool still_runs = kill(sub_c.id(), 0) == 0;
#else 
    bool still_runs = sub_c.running();
#endif

    BOOST_CHECK(still_runs);
    if (still_runs)
        sub_c.terminate();

    BOOST_CHECK(!c.running());
    if (c.running())
        c.terminate();

    std::cerr << "detached out" << std::endl;
}
