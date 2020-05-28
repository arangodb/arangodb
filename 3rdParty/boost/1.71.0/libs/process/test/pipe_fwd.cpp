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


#include <string>
#include <istream>
#include <iostream>
#include <cstdlib>

BOOST_AUTO_TEST_SUITE( pipe_tests );

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(sync_io, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;


    bp::ipstream is;
    bp::opstream os;

    bp::pipe p;

    std::error_code ec;
    bp::child c1(
        master_test_suite().argv[1],
        bp::args={"test", "--prefix-once", "dear "},
        bp::std_in<os,
        bp::std_out>p,
        ec
    );
    BOOST_REQUIRE(!ec);

    BOOST_TEST_INFO("Launching child 2");

    bp::child c2(
        master_test_suite().argv[1],
        bp::args={"test", "--prefix-once", "hello "},
        bp::std_in<p,
        bp::std_out>is,
        ec
    );
    BOOST_REQUIRE(!ec);

    os << "boost-user!" << std::endl;


    std::string s;
    std::getline(is, s);

    std::string cmp = "hello dear boost-user!";

    s.resize(cmp.size());

    BOOST_CHECK_EQUAL_COLLECTIONS(s.cbegin(), s.cend(),cmp.cbegin(), cmp.cend());


}

BOOST_AUTO_TEST_SUITE_END();