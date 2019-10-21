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

#include <boost/process/exe.hpp>
#include <boost/process/args.hpp>
#include <boost/process/cmd.hpp>
#include <boost/process/io.hpp>
#include <boost/process/error.hpp>
#include <boost/process/child.hpp>

#include <boost/algorithm/string/predicate.hpp>

namespace bp = boost::process;

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(args, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream is;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--echo-argv",
        bp::args+={"hello thingy", "\"stuff\""},
        bp::args+="  spa ce  ",
        bp::std_out>is,
        ec
    );
    if (ec)
        std::cout << "EC: " << ec.message() << std::endl;
    BOOST_REQUIRE(!ec);

    std::string s;

    std::getline(is, s);
    s.resize(4);
    BOOST_CHECK_EQUAL(s, "test");

    std::getline(is, s);
    s.resize(11);
    BOOST_CHECK_EQUAL(s, "--echo-argv");

    std::getline(is, s);
    s.resize(12);

    BOOST_CHECK_EQUAL(s, "hello thingy");

    std::getline(is, s);
    s.resize(7);

    BOOST_CHECK_EQUAL(s, "\"stuff\"");

    std::getline(is, s);
    s.resize(10);

    BOOST_CHECK_EQUAL(s, "  spa ce  ");

}


BOOST_AUTO_TEST_CASE(cmd, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream is;

    std::error_code ec;

    std::string cmd = master_test_suite().argv[1];
    cmd+= " test --echo-argv \"hello thingy\" \\\"stuff\\\" \"  spa ce  \"";

    bp::child c(cmd,
        bp::std_out>is,
        ec
    );
    BOOST_REQUIRE(!ec);

    std::string s;

    std::getline(is, s);
    s.resize(4);
    BOOST_CHECK_EQUAL(s, "test");

    std::getline(is, s);
    s.resize(11);
    BOOST_CHECK_EQUAL(s, "--echo-argv");

    std::getline(is, s);
    s.resize(12);

    BOOST_CHECK_EQUAL(s, "hello thingy");

    std::getline(is, s);
    s.resize(7);

    BOOST_CHECK_EQUAL(s, "\"stuff\"");

    std::getline(is, s);
    s.resize(10);

    BOOST_CHECK_EQUAL(s, "  spa ce  ");
}
