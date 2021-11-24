// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <vector>
#include "../b2_workarounds.hpp"

//[callplugcpp_tutorial7
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include <iostream>

void load_and_execute(const boost::dll::fs::path libraries[], std::size_t libs_count) {
    const std::string username = "User";

    for (std::size_t i = 0; i < libs_count; ++i) {
        // Class `library_info` can extract information from a library
        boost::dll::library_info inf(libraries[i]);

        // Getting symbols exported from 'Anna' section
        std::vector<std::string> exports = inf.symbols("Anna");

        // Loading library and importing symbols from it
        boost::dll::shared_library lib(libraries[i]);
        for (std::size_t j = 0; j < exports.size(); ++j) {
            std::cout << "\nFunction '" << exports[j] << "' prints:\n\t";
            lib.get_alias<void(const std::string&)>(exports[j]) // importing function
                (username);                                     // calling function
        }
    }
}
//]

int main(int argc, char* argv[]) {
    /*<-*/ BOOST_ASSERT(argc >= 3); /*->*/
    std::vector<boost::dll::fs::path> libraries;
    libraries.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        if (b2_workarounds::is_shared_library(argv[i])) {
            libraries.push_back(argv[i]);
        }
    }

    load_and_execute(&libraries[0], libraries.size());
}
