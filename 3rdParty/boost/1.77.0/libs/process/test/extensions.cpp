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
#include <system_error>
#include <boost/bind.hpp>
#include <boost/ref.hpp>
#include <boost/process/child.hpp>
#include <boost/process/extend.hpp>


namespace bp = boost::process;


struct run_exe
{
    std::string exe;
    template<typename T>
    void operator()(T &e) const
    {
        e.exe = exe.c_str();
    }
};

struct  set_on_error
{
    mutable std::error_code ec;
    template<typename T>
    void operator()(T &, const std::error_code & ec) const
    {
        this->ec = ec;
    }
};

BOOST_AUTO_TEST_CASE(extensions)
{
    using boost::unit_test::framework::master_test_suite;

    run_exe re;

    re.exe = master_test_suite().argv[1];

    set_on_error se;
    std::error_code ec;
    bp::child c(
        "Wrong-Command",
        "test",
        bp::extend::on_setup=re,
        bp::extend::on_error=se,
        bp::ignore_error
    );
    BOOST_CHECK(!se.ec);
}


namespace ex = boost::process::extend;


std::string st = "not called";

struct overload_handler : ex::handler
{
    template <class Char, class Sequence>
    void on_setup(ex::windows_executor<Char, Sequence>& exec) const
    {
        st = "windows";
    }
    template <class Sequence>
    void on_setup(ex::posix_executor<Sequence>& exec) const
    {
        st = "posix";
    }
};

BOOST_AUTO_TEST_CASE(overload)
{
    bp::child c(
        overload_handler(),
        bp::ignore_error
    );
#if defined(BOOST_WINDOWS_API)
    BOOST_CHECK_EQUAL(st, "windows");
#else
    BOOST_CHECK_EQUAL(st, "posix");
#endif
}



