// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <boost/asio.hpp>
#include <boost/array.hpp>
#include <string>

namespace bp = boost::process;

int main()
{
    boost::asio::io_context ios;
    boost::asio::streambuf buffer;


    bp::child c(
        "test.exe",
        bp::std_out > buffer,
        ios
        );

    ios.run();
}
