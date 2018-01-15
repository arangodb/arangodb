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


#include <boost/asio.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/process/error.hpp>
#include <boost/process/async_system.hpp>
#include <system_error>

namespace bp = boost::process;;

void fail_func()
{
    boost::asio::io_context ios;

    bp::async_system(ios, boost::asio::use_future, "foo", bp::ignore_error);
}
