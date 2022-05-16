//  directory_symlink_parent_resolution.cpp  -------------------------------------------//

//  Copyright Beman Dawes 2015

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  Library home page: http://www.boost.org/libs/filesystem

#include <boost/filesystem.hpp>
#include <boost/filesystem/string_file.hpp>
#include <boost/detail/lightweight_main.hpp>
#include <iostream>
#include <string>

using std::cout;
using std::endl;
using namespace boost::filesystem;

int cpp_main(int argc, char* argv[])
{
#ifdef BOOST_WINDOWS_API
    cout << "BOOST_WINDOWS_API" << endl;
#else
    cout << "BOOST_POSIX_API" << endl;
#endif

    path test_dir(current_path() / "dspr_demo");

    remove_all(test_dir);
    create_directories(test_dir / "a/c/d");
    current_path(test_dir / "a");
    create_directory_symlink("c/d", "b");
    save_string_file("name.txt", "Windows");
    save_string_file("c/name.txt", "POSIX");
    current_path(test_dir);
    std::string s;
    load_string_file("a/b/../name.txt", s);
    cout << s << endl;

    return 0;
}
