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
#include <boost/process/search_path.hpp>
#include <boost/filesystem/path.hpp>
#include <string>

namespace bp = boost::process;
namespace fs = boost::filesystem;

BOOST_AUTO_TEST_CASE(search_path)
{
#if defined(BOOST_WINDOWS_API)
    std::string filename = "cmd";
#elif defined(BOOST_POSIX_API)
    fs::path filename = "ls";
#endif

    BOOST_CHECK(!bp::search_path(filename).empty());
    auto fs = bp::search_path(filename);

    std::cout << fs << std::endl;

#if defined(BOOST_WINDOWS_API)
    std::vector<fs::path> path = {"C:\\Windows","C:\\Windows\\System32"};
#elif defined(BOOST_POSIX_API)
    std::vector<fs::path> path = {"/usr/local/bin","/usr/bin","/bin"};
#endif

    BOOST_CHECK(!bp::search_path(filename, path).empty());
}
