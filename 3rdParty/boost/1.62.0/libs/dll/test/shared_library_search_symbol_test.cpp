// Copyright 2011-2012 Renato Tegon Forti
// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../example/b2_workarounds.hpp"
#include <boost/dll.hpp>
#include <boost/core/lightweight_test.hpp>

// Unit Tests

extern "C" void BOOST_SYMBOL_EXPORT exef() {
}

int main(int argc, char* argv[])
{
    using namespace boost::dll;

    boost::filesystem::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("test_library") != std::string::npos);
    BOOST_TEST(b2_workarounds::is_shared_library(shared_library_path));
    std::cout << "Library: " << shared_library_path;

    {
        shared_library sl(shared_library_path);
        BOOST_TEST(sl.has("say_hello"));
        BOOST_TEST(sl.has("lib_version"));
        BOOST_TEST(sl.has("integer_g"));
        BOOST_TEST(sl.has(std::string("integer_g")));
        BOOST_TEST(!sl.has("i_do_not_exist"));
        BOOST_TEST(!sl.has(std::string("i_do_not_exist")));
    }
   
    {
        shared_library sl(program_location());
        BOOST_TEST(sl.has("exef"));
        BOOST_TEST(!sl.has("i_do_not_exist"));
    }


    exef(); // Make sure that this function still callable in traditional way
    return boost::report_errors();
}

