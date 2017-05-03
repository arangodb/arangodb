// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[plugcpp_my_plugin_refcounting
#include "refcounting_plugin.hpp"
#include <boost/dll/runtime_symbol_info.hpp> // for this_line_location()

namespace my_namespace {

class my_plugin_refcounting : public my_refcounting_api {
public:    
    // Must be instantiated in plugin
    boost::filesystem::path location() const {
        return boost::dll::this_line_location(); // location of this plugin
    }

    std::string name() const {
        return "refcounting";
    }

    // ...
    //<-
    // This block is invisible for Quickbook documentation
    float calculate(float /*x*/, float /*y*/) {
        return 0;
    }
    //->
};

} // namespace my_namespace

// Factory method. Returns *simple pointer*!
my_refcounting_api* create() {
    return new my_namespace::my_plugin_refcounting();
}

//]



