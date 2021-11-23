// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/core/lightweight_test.hpp>

#include <boost/process.hpp>
#include <boost/process/cmd.hpp>
#include <system_error>

namespace bp = boost::process;

int main(int argc, char* argv[])
{
    bool thrown = false;
    try {
        bp::child c(
            bp::cmd="doesnt-exist",
            bp::throw_on_error
        );
        thrown = false;
    }
    catch(bp::process_error & )
    {
        thrown = true;
    }
    BOOST_TEST(thrown);
    return boost::report_errors();
}

