// Copyright 2011-2012 Renato Tegon Forti
// Copyright 2015-2021 Antony Polukhin
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
    boost::dll::fs::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
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

#if defined(__GNUC__) && __GNUC__ >= 4 && defined(__ELF__)
    BOOST_TEST(std::find(symb.begin(), symb.end(), "protected_function") != symb.end());
#endif
    
    symb = lib_info.symbols("boostdll");
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    BOOST_TEST(std::find(symb.begin(), symb.end(), "const_integer_g_alias") != symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "foo_variable") != symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "const_integer_g") == symb.end());
    BOOST_TEST(std::find(symb.begin(), symb.end(), "say_hello") == symb.end());
    BOOST_TEST(lib_info.symbols(std::string("boostdll")) == symb);

    std::vector<std::string> empty = lib_info.symbols("empty");
    BOOST_TEST(empty.empty() == true);

    BOOST_TEST(lib_info.symbols("section_that_does_not_exist").empty());

    // Self testing
    std::cout << "Self: " << argv[0];
    boost::dll::library_info self_info(argv[0]);

    sec = self_info.sections();
    //std::copy(sec.begin(), sec.end(), std::ostream_iterator<std::string>(std::cout, ",  "));
    BOOST_TEST(std::find(sec.begin(), sec.end(), "boostdll") != sec.end());

    symb = self_info.symbols("boostdll");
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
    BOOST_TEST(std::find(symb.begin(), symb.end(), "create_plugin") != symb.end());

    boost::dll::fs::path sections_stripped_path = "/lib/x86_64-linux-gnu/libaio.so.1";
    if (exists(sections_stripped_path)) {
        boost::dll::library_info stripped_lib(sections_stripped_path);
        std::cout << "\n\n\n";
        symb = stripped_lib.symbols();
        std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));
        BOOST_TEST(!symb.empty());
    }

    return boost::report_errors();
}
