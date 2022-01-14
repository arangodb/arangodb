// Copyright 2011-2012 Renato Tegon Forti.
// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../example/b2_workarounds.hpp"
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/function.hpp>


// Unit Tests
int main(int argc, char* argv[]) {
    using namespace boost::dll;

    boost::dll::fs::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("test_library") != std::string::npos);
    BOOST_TEST(b2_workarounds::is_shared_library(shared_library_path));
    boost::dll::fs::path bad_path = shared_library_path / "directory_that_does_not_exist";

    try {
        shared_library lib(bad_path);
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        shared_library lib;
        lib.get<int>("variable_or_function_that_does_not_exist");
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        shared_library lib("");
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        shared_library lib("\0\0");
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        shared_library lib;
        lib.location();
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        shared_library lib;
        lib.load("\0\0", load_mode::rtld_global);
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    shared_library sl(shared_library_path);
    try {
        sl.get<int>("variable_or_function_that_does_not_exist");
        BOOST_TEST(false);
    } catch (const boost::dll::fs::system_error& e) {
        std::cout << e.what() << '\n';
    }

    try {
        library_info lib("\0");
        BOOST_TEST(false);
    } catch (const std::exception& e) {
        std::cout << e.what() << '\n';
    }

    try {
        std::string not_a_binary(argv[1]);
        not_a_binary += "/not_a_binary";
        std::ofstream ofs(not_a_binary.c_str());
        ofs << "This is not a binary file, so library_info must report 'Unsupported binary format'";
        ofs.close();
        library_info lib(not_a_binary);
        BOOST_TEST(false);
    } catch (const std::exception& e) {
        std::cout << e.what() << '\n';
    }
    return boost::report_errors();
}

