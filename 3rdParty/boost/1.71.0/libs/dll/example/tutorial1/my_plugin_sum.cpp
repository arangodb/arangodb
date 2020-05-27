// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>


//[plugcpp_my_plugin_sum
#include <boost/config.hpp> // for BOOST_SYMBOL_EXPORT
#include "../tutorial_common/my_plugin_api.hpp"

namespace my_namespace {

class my_plugin_sum : public my_plugin_api {
public:
    my_plugin_sum() {
        std::cout << "Constructing my_plugin_sum" << std::endl;
    }

    std::string name() const {
        return "sum";
    }

    float calculate(float x, float y) {
        return x + y;
    }
   
    ~my_plugin_sum() {
        std::cout << "Destructing my_plugin_sum ;o)" << std::endl;
    }
};

// Exporting `my_namespace::plugin` variable with alias name `plugin`
// (Has the same effect as `BOOST_DLL_ALIAS(my_namespace::plugin, plugin)`)
extern "C" BOOST_SYMBOL_EXPORT my_plugin_sum plugin;
my_plugin_sum plugin;

} // namespace my_namespace

//]
