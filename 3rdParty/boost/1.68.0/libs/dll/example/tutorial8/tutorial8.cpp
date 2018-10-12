// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2017 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp"

//[callplugcpp_tutorial8
#include <iostream>
#include "refcounting_api.hpp"

int main(int argc, char* argv[]) {
    /*<-*/ b2_workarounds::argv_to_path_guard guard(argc, argv); /*->*/
    boost::shared_ptr<my_refcounting_api> plugin = get_plugin(
        boost::filesystem::path(argv[1]) / "refcounting_plugin",
        "create_refc_plugin"
    );

    std::cout << "Plugin name: " << plugin->name() 
              << ", \nlocation: " << plugin->location() 
              << std::endl;
}
//]
