// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <boost/process/windows.hpp>
#include <iostream>

#include <windows.h>

namespace bp = boost::process;

int main()
{
    bp::system("test.exe",
        bp::windows::show);


    bp::system("test.exe",
        bp::on_setup([](auto &e)
            { e.startup_info.dwFlags = STARTF_RUNFULLSCREEN; }),
        bp::on_error([](auto&, const std::error_code & ec)
            { std::cerr << ec.message() << std::endl; })
        );
}
