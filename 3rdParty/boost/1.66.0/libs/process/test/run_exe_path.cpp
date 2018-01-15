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
#include <boost/filesystem.hpp>

#include <boost/process/cmd.hpp>
#include <boost/process/error.hpp>
#include <boost/process/child.hpp>

namespace bp = boost::process;


BOOST_AUTO_TEST_CASE(run_exe_success)
{
    using boost::unit_test::framework::master_test_suite;

    boost::filesystem::path exe = master_test_suite().argv[1];

    std::error_code ec;
    bp::child c(
        exe,
        ec
    );
    BOOST_CHECK(!ec);
}

#if defined(BOOST_WINDOWS_API)
BOOST_AUTO_TEST_CASE(run_exe_error)
{
    boost::filesystem::path exe = "doesnt-exist";

    std::error_code ec;
    bp::child c(
        exe,
        ec
    );
    BOOST_CHECK(ec);
}
#endif
