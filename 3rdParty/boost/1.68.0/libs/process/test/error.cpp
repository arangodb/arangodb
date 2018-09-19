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
#include <boost/process/child.hpp>
#include <boost/process/extend.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <system_error>



namespace bp = boost::process;

struct err_set
{
    std::error_code ec;
    const char* msg = "";

    template<typename Exec>
    void operator()(Exec& exec)
    {
        exec.set_error(ec, msg);
    }

    template<typename Exec>
    void operator()(Exec& exec, const std::error_code &)
    {
        exec.set_error(ec, msg);
    }
};

BOOST_AUTO_TEST_CASE(setup_error)
{
    using boost::unit_test::framework::master_test_suite;

    err_set es;

    {
        es.ec.assign(42, std::system_category());
        std::error_code ec;
        bp::child c(master_test_suite().argv[1], ec, bp::extend::on_setup(es));

        BOOST_CHECK(!c.running());
        BOOST_CHECK(ec);
        BOOST_CHECK_EQUAL(ec.value(), 42);
    }
    bool has_thrown = false;
    try
    {
        es.ec.assign(24, std::system_category());
        es.msg = "MyMessage";
        bp::child c(master_test_suite().argv[1], bp::extend::on_setup(es));

    }
    catch( bp::process_error & se)
    {
        has_thrown = true;
        BOOST_CHECK_EQUAL(se.code().value(), 24);
        BOOST_CHECK(boost::starts_with(se.what(), "MyMessage"));
    }
    BOOST_CHECK(has_thrown);
}

BOOST_AUTO_TEST_CASE(success_error)
{
    using boost::unit_test::framework::master_test_suite;

    err_set es;

    {
        es.ec.assign(22, std::system_category());
        std::error_code ec;
        bp::child c(master_test_suite().argv[1], ec, bp::extend::on_success(es));


        BOOST_CHECK(!c.running());
        BOOST_CHECK(ec);
        BOOST_CHECK_EQUAL(ec.value(), 22);
        std::cout << "Value: " << ec.value() << std::endl;
    }
    bool has_thrown = false;
    try
    {
        es.ec.assign(23, std::system_category());
        es.msg = "MyMessage";
        bp::child c(master_test_suite().argv[1], bp::extend::on_success(es));


    }
    catch( bp::process_error & se)
    {
        has_thrown = true;
        BOOST_CHECK_EQUAL(se.code().value(), 23);
        BOOST_CHECK(boost::starts_with(se.what(), "MyMessage"));
    }
    BOOST_CHECK(has_thrown);
}

BOOST_AUTO_TEST_CASE(ignore_error)
{

    {
        BOOST_CHECK_NO_THROW(bp::child c("doesnt-exit", bp::ignore_error));
    }
}
