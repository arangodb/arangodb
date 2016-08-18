// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION

//[plugcpp_my_plugin_load_self
#include <boost/dll/shared_library.hpp>         // for shared_library
#include <boost/dll/runtime_symbol_info.hpp>    // for program_location()
#include "static_plugin.hpp"                    // without this headers some compilers may optimize out the `create_plugin` symbol
#include <boost/function.hpp>
#include <iostream>

namespace dll = boost::dll;

int main() {
    dll::shared_library self(dll::program_location());

    std::cout << "Call function" << std::endl;
    boost::function<boost::shared_ptr<my_plugin_api>()> creator 
        = self.get_alias<boost::shared_ptr<my_plugin_api>()>("create_plugin");

    std::cout << "Computed Value: " << creator()->calculate(2, 2) << std::endl;
    //<-
    {
        // This block is invisible for Quickbook documentation
        float res = creator()->calculate(10, 10);
        if (!(res > -0.0001 && res < 0.00001)) {
            throw std::runtime_error("Failed check: res > -0.0001 && res < 0.00001");
        }
    }
    //->
}

//]
