// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp"

//[callplugcpp_tutorial6
#include <boost/dll/import.hpp>
#include <boost/function.hpp>
#include <iostream>

typedef boost::function<void()> callback_t;

void print_unloaded() {
    std::cout << "unloaded" << std::endl;
}

int main(int argc, char* argv[]) {
    // argv[1] contains full path to our plugin library
    boost::dll::fs::path shared_library_path = /*<-*/ b2_workarounds::first_lib_from_argv(argc, argv); /*->*/ //=argv[1];

    // loading library and getting a function from it
    boost::function<void(const callback_t&)> on_unload
        = boost::dll::import_alias<void(const callback_t&)>(
            shared_library_path, "on_unload"
        );

    on_unload(&print_unloaded); // adding a callback
    std::cout << "Before library unload." << std::endl;

    // Releasing last reference to the library, so that it gets unloaded
    on_unload.clear();
    std::cout << "After library unload." << std::endl;
}
//]

