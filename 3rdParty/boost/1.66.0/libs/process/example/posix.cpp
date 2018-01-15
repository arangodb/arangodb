// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/process.hpp>
#include <boost/process/posix.hpp>
#include <boost/process/extend.hpp>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <errno.h>

namespace bp = boost::process;

int main()
{

    //duplicate our pipe descriptor into literal position 4
    bp::pipe p;
    bp::system("test", bp::posix::fd.bind(4, p.native_sink()));


    //close file-descriptor from explicit integral value
    bp::system("test", bp::posix::fd.close(STDIN_FILENO));

    //close file-descriptors from explicit integral values
    bp::system("test", bp::posix::fd.close({STDIN_FILENO, STDOUT_FILENO}));

    //add custom handlers
    const char *env[2] = { 0 };
    env[0] = "LANG=de";
    bp::system("test",
        bp::extend::on_setup([env](auto &e) { e.env = const_cast<char**>(env); }),
        bp::extend::on_fork_error([](auto&, const std::error_code & ec)
            { std::cerr << errno << std::endl; }),
        bp::extend::on_exec_setup([](auto&)
            { ::chroot("/new/root/directory/"); }),
        bp::extend::on_exec_error([](auto&, const std::error_code & ec)
            { std::ofstream ofs("log.txt"); if (ofs) ofs << errno; })
    );

}
