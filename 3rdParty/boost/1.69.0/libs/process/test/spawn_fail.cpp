// Copyright (c) 2006, 2007 Julio M. Merino Vidal
// Copyright (c) 2008 Ilya Sokolov, Boris Schaeling
// Copyright (c) 2009 Boris Schaeling
// Copyright (c) 2010 Felipe Tanus, Boris Schaeling
// Copyright (c) 2011, 2012 Jeff Flinn, Boris Schaeling
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/system/error_code.hpp>

#include <boost/asio.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <boost/process/error.hpp>
#include <boost/process/io.hpp>
#include <boost/process/args.hpp>
#include <boost/process/spawn.hpp>
#include <boost/process/async_pipe.hpp>
#include <boost/process/async.hpp>
#include <system_error>

#include <boost/filesystem.hpp>

#include <string>
#include <istream>
#include <cstdlib>
#if defined(BOOST_WINDOWS_API)
#   include <windows.h>
typedef boost::asio::windows::stream_handle pipe_end;
#elif defined(BOOST_POSIX_API)
#   include <sys/wait.h>
#   include <unistd.h>
typedef boost::asio::posix::stream_descriptor pipe_end;
#endif

namespace fs = boost::filesystem;
namespace bp = boost::process;

int main()
{
    std::error_code ec;

    boost::asio::io_context ios;

    bp::spawn(
        "dummy",
        bp::on_exit([](int, const std::error_code&){}),
        ios,
        ec
    ); 
}



