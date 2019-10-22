// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "../b2_workarounds.hpp"
//[callplugcpp_tutorial3
#include <boost/dll/import.hpp> // for import_alias
#include <boost/make_shared.hpp>
#include <boost/function.hpp>
#include <iostream>
#include "../tutorial_common/my_plugin_api.hpp"

namespace dll = boost::dll;

std::size_t search_for_symbols(const std::vector<boost::dll::fs::path>& plugins) {
    std::size_t plugins_found = 0;

    for (std::size_t i = 0; i < plugins.size(); ++i) {
        std::cout << "Loading plugin: " << plugins[i] << '\n';
        dll::shared_library lib(plugins[i], dll::load_mode::append_decorations);
        if (!lib.has("create_plugin")) {
            // no such symbol
            continue;
        }

        // library has symbol, importing...
        typedef boost::shared_ptr<my_plugin_api> (pluginapi_create_t)();
        boost::function<pluginapi_create_t> creator
            = dll::import_alias<pluginapi_create_t>(boost::move(lib), "create_plugin");

        std::cout << "Matching plugin name: " << creator()->name() << std::endl;
        ++ plugins_found;
    }

    return plugins_found;
}

//]

int main(int argc, char* argv[]) { 
    BOOST_ASSERT(argc >= 3);
    std::vector<boost::dll::fs::path> plugins;
    plugins.reserve(argc - 1);
    for (int i = 1; i < argc; ++i) {
        if (b2_workarounds::is_shared_library(argv[i])) {
            plugins.push_back(argv[i]);
        }
    }

    const std::size_t res = search_for_symbols(plugins);
    BOOST_ASSERT(res == 1);
    (void)res;
}
