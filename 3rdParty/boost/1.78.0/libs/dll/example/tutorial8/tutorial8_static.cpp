// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[callplugcpp_tutorial8_static
#include <boost/dll/runtime_symbol_info.hpp> // program_location()
#include <iostream>
#include "refcounting_plugin.hpp"

int main() {
    boost::shared_ptr<my_refcounting_api> plugin = get_plugin(
        boost::dll::program_location(),
        "create_refc_plugin"
    );

    std::cout << "Plugin name: " << plugin->name() 
              << ", \nlocation: " << plugin->location() 
              << std::endl;
}
//]
