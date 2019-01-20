// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp"

//[callplugcpp_tutorial2
#include <boost/dll/import.hpp> // for import_alias
#include <boost/function.hpp>
#include <iostream>
#include "../tutorial_common/my_plugin_api.hpp"

namespace dll = boost::dll;

int main(int argc, char* argv[]) {
    /*<-*/ b2_workarounds::argv_to_path_guard guard(argc, argv); /*->*/
    boost::filesystem::path shared_library_path(argv[1]);               // argv[1] contains path to directory with our plugin library
    shared_library_path /= "my_plugin_aggregator";
    typedef boost::shared_ptr<my_plugin_api> (pluginapi_create_t)();
    boost::function<pluginapi_create_t> creator;

    creator = boost::dll::import_alias<pluginapi_create_t>(             // type of imported symbol must be explicitly specified
        shared_library_path,                                            // path to library
        "create_plugin",                                                // symbol to import
        dll::load_mode::append_decorations                              // do append extensions and prefixes
    );

    boost::shared_ptr<my_plugin_api> plugin = creator();
    std::cout << "plugin->calculate(1.5, 1.5) call:  " << plugin->calculate(1.5, 1.5) << std::endl;
    std::cout << "plugin->calculate(1.5, 1.5) second call:  " << plugin->calculate(1.5, 1.5) << std::endl;
    std::cout << "Plugin Name:  " << plugin->name() << std::endl;
}
//]
