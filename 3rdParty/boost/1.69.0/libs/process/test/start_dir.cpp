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
#include <boost/process.hpp>
#include <boost/system/error_code.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/compare.hpp>
#include <string>
#include <iostream>

namespace bp  = boost::process;


struct test_dir
{
    std::string s_;
    test_dir(const std::string &s) : s_(s)
    { BOOST_REQUIRE_NO_THROW(boost::filesystem::create_directory(s)); }
    ~test_dir() { boost::filesystem::remove(s_); }
};

BOOST_AUTO_TEST_CASE(start_in_dir)
{
    using boost::unit_test::framework::master_test_suite;

    test_dir dir("start_in_dir_test");

    bp::ipstream is;

    std::error_code ec;
    bp::child c(
        bp::exe=boost::filesystem::absolute(master_test_suite().argv[1]).string(),
        bp::args +={"test", "--pwd"},
        bp::start_dir = dir.s_,
        bp::std_out>is,
        ec
    );
    BOOST_REQUIRE(!ec);


    std::string s;
    std::getline(is, s);
    auto path_read = boost::filesystem::absolute(boost::filesystem::path(s)).string();
    auto path_set  = boost::filesystem::absolute(dir.s_).string();

    if (path_read.size() > path_set.size())
        path_read.resize(path_set.size());
    else if (path_read.size() < path_set.size())
        path_set.resize(path_read.size());

    BOOST_CHECK_EQUAL_COLLECTIONS(path_read.begin(), path_read.end(),
                                  path_set.begin(), path_set.end());

    BOOST_REQUIRE_NO_THROW(c.wait());
}
