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

#include <boost/process/args.hpp>
#include <boost/process/child.hpp>
#include <boost/process/env.hpp>
#include <boost/process/environment.hpp>
#include <boost/process/error.hpp>
#include <boost/process/io.hpp>

#include <boost/algorithm/string/predicate.hpp>

#include <boost/system/error_code.hpp>

#include <boost/program_options/environment_iterator.hpp>
#include <string>
#include <stdlib.h>
#include <list>

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(inherit_env, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream st;

    std::error_code ec;

    bp::child c(
        master_test_suite().argv[1],
        "test", "--query", "PATH",
        bp::std_out>st,
        ec
    );
    BOOST_REQUIRE(!ec);

    std::string s;

    std::getline(st, s);

    auto path = boost::this_process::environment()["PATH"].to_string();

    std::cout << "Path : '" << path << "'" << std::endl;
    std::cout << "Value: '" << s    << "'" << std::endl;

    if(!path.empty())
    {
        auto size = (path.size() < s.size()) ? path.size() : s.size();

        BOOST_CHECK_EQUAL_COLLECTIONS(
            s.begin(),    s.   begin() + size,
            path.begin(), path.begin() + size
            );
    }
    else
        BOOST_CHECK(boost::starts_with(s, "************** empty environment **************"));
    c.wait();
}


BOOST_AUTO_TEST_CASE(inherit_mod_env, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    auto ie = boost::this_process::environment();
    std::string value = "TestString";
    ie["BOOST_PROCESS_TEST_1"] = value;

    {
        auto ie2 = boost::this_process::environment();
        auto val = ie2["BOOST_PROCESS_TEST_1"];
        auto st  = val.to_string();

        BOOST_CHECK_EQUAL_COLLECTIONS(
            st.begin(),     st.end(),
            value.begin(), value.end()
            );
    }
    bp::ipstream st;

    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--query", "BOOST_PROCESS_TEST_1",
        bp::std_out>st,
        ec
    );
    BOOST_REQUIRE(!ec);

    std::string s;

    std::getline(st, s);

    auto size = (value.size() < s.size()) ? value.size() : s.size();

    BOOST_CHECK_EQUAL_COLLECTIONS(
            s.begin(),     s.    begin() + size,
            value.begin(), value.begin() + size
            );
    c.wait();
}


BOOST_AUTO_TEST_CASE(modifided_env, *boost::unit_test::timeout(2))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream st;

    boost::process::environment env = boost::this_process::environment(); //empty env, that would fail.
    std::string value = "TestString";
    env["BOOST_PROCESS_TEST_2"] = value;


    std::error_code ec;
    bp::child c(
        master_test_suite().argv[1],
        "test", "--query", "BOOST_PROCESS_TEST_2",
        bp::std_out>st,
        env,
        ec
    );
    BOOST_REQUIRE(!ec);
    BOOST_REQUIRE(boost::this_process::environment().count(value) == 0);

    std::string s;
    std::getline(st, s);

    BOOST_CHECK(boost::algorithm::starts_with(s, "TestString"));
    c.wait();
}


BOOST_AUTO_TEST_CASE(append, *boost::unit_test::timeout(5))
{
    using boost::unit_test::framework::master_test_suite;

    bp::ipstream st;
    BOOST_TEST_PASSPOINT();
    bp::environment e = boost::this_process::environment();

    std::error_code ec;
    BOOST_REQUIRE_GE(e.size(), 1u);

    std::list<std::string> arg = {"test", "--query", "BOOST_PROCESS_TEST_3"};
    bp::child c(
        master_test_suite().argv[1],
        bp::env["BOOST_PROCESS_TEST_3"]="some_string",
        bp::env=e,
        bp::env["BOOST_PROCESS_TEST_3"]=boost::none,
        bp::env["BOOST_PROCESS_TEST_3"]+="some_fictional_path_42",
        bp::env["BOOST_PROCESS_TEST_3"]+={"other", "next"},
        bp::args=arg,
        bp::std_out>st,
        ec
    );

    BOOST_REQUIRE(!ec);
    BOOST_WARN(c.running());
    std::string s;

    std::getline(st, s);
#if defined(BOOST_WINDOWS_API)
    BOOST_CHECK(boost::starts_with(s, "some_fictional_path_42;other;next"));
#else
    BOOST_CHECK(boost::starts_with(s, "some_fictional_path_42:other:next"));
#endif
    c.wait();
}
