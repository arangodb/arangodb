// Copyright 2011-2012 Renato Tegon Forti
// Copyright 2015 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION
#include "../example/b2_workarounds.hpp"

#include <boost/dll/library_info.hpp>
#include <boost/core/lightweight_test.hpp>
#include "../example/tutorial4/static_plugin.hpp"

// Unit Tests

#include <iterator>

int main(int argc, char* argv[])
{
    boost::filesystem::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("test_library") != std::string::npos);

    boost::dll::library_info lib_info(shared_library_path);
    std::vector<std::string> sec = lib_info.sections();
    std::copy(sec.begin(), sec.end(), std::ostream_iterator<std::string>(std::cout, ",  "));
    BOOST_TEST(std::find(sec.begin(), sec.end(), "boostdll") != sec.end());


    std::cout << "\n\n\n";
    std::vector<std::string> symb = lib_info.symbols();
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    BOOST_TEST(std::find(symb.begin(), symb.end(), "const_integer_g") != symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "say_hello") != symb.end());
    
    symb = lib_info.symbols("boostdll");
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    BOOST_TEST(std::find(symb.begin(), symb.end(), "const_integer_g_alias") != symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "foo_variable") != symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "const_integer_g") == symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "say_hello") == symb.end());
    BOOST_TEST(lib_info.symbols(std::string("boostdll")) == symb);


    // Self testing
    std::cout << "Self: " << argv[0];
    boost::dll::library_info self_info(argv[0]);

    sec = self_info.sections();
    //std::copy(sec.begin(), sec.end(), std::ostream_iterator<std::string>(std::cout, ",  "));
    BOOST_TEST(std::find(sec.begin(), sec.end(), "boostdll") != sec.end());

    symb = self_info.symbols("boostdll");
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    BOOST_TEST(std::find(symb.begin(), symb.end(), "create_plugin") != symb.end());

    return boost::report_errors();
}
