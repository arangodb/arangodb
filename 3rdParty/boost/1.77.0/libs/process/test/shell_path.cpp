// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_TEST_MAIN
#include <boost/test/included/unit_test.hpp>
#include <boost/process/shell.hpp>
#include <boost/filesystem.hpp>
#include <system_error>

namespace bp = boost::process;

BOOST_AUTO_TEST_CASE(shell_set_on_error)
{
    std::error_code ec;
    boost::filesystem::path p = bp::shell(ec);
    BOOST_CHECK(!ec);
    BOOST_CHECK(boost::filesystem::exists(p));
}

BOOST_AUTO_TEST_CASE(shell_throw_on_error)
{
    BOOST_CHECK_NO_THROW(bp::shell());
    BOOST_CHECK(boost::filesystem::exists(bp::shell()));
}
