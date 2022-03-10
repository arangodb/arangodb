// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp" // contains dll_test::replace_with_full_path

//[callplugcpp_tutorial1
#include <boost/dll/import.hpp> // for import_alias
#include <iostream>
#include "../tutorial_common/my_plugin_api.hpp"

namespace dll = boost::dll;

int main(int argc, char* argv[]) {
    /*<-*/ b2_workarounds::argv_to_path_guard guard(argc, argv); /*->*/
    boost::dll::fs::path lib_path(argv[1]);             // argv[1] contains path to directory with our plugin library
    boost::shared_ptr<my_plugin_api> plugin;            // variable to hold a pointer to plugin variable
    std::cout << "Loading the plugin" << std::endl;
     
    plugin = dll::import_symbol<my_plugin_api>(         // type of imported symbol is located between `<` and `>`
        lib_path / "my_plugin_sum",                     // path to the library and library name
        "plugin",                                       // name of the symbol to import
        dll::load_mode::append_decorations              // makes `libmy_plugin_sum.so` or `my_plugin_sum.dll` from `my_plugin_sum`
    );

    std::cout << "plugin->calculate(1.5, 1.5) call:  " << plugin->calculate(1.5, 1.5) << std::endl;
}
//]
