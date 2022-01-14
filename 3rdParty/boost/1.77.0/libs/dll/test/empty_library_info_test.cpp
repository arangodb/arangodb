// Copyright 2020-2021 Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#include "../example/b2_workarounds.hpp"

#include <boost/dll/library_info.hpp>
#include <boost/core/lightweight_test.hpp>

// Unit Tests

#include <iterator>

int main(int argc, char* argv[])
{
    boost::dll::fs::path shared_library_path = b2_workarounds::first_lib_from_argv(argc, argv);
    BOOST_TEST(shared_library_path.string().find("empty_library") != std::string::npos);

    boost::dll::library_info lib_info(shared_library_path);
    std::vector<std::string> sec = lib_info.sections();
    std::copy(sec.begin(), sec.end(), std::ostream_iterator<std::string>(std::cout, ",  "));

    std::cout << "\n\n\n";
    std::vector<std::string> symb = lib_info.symbols();
    std::copy(symb.begin(), symb.end(), std::ostream_iterator<std::string>(std::cout, "\n"));

    symb = lib_info.symbols("boostdll");
    BOOST_TEST(symb.empty());

    std::vector<std::string> empty = lib_info.symbols("empty");
    BOOST_TEST(empty.empty());

    BOOST_TEST(lib_info.symbols("section_that_does_not_exist").empty());

    return boost::report_errors();
}
