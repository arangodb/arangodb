// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <iostream>
#include <boost/make_shared.hpp>

// MinGW related workaround
#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION

//[plugcpp_my_plugin_aggregator
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS   
#include "../tutorial_common/my_plugin_api.hpp"

namespace my_namespace {

class my_plugin_aggregator : public my_plugin_api {
    float aggr_;
    my_plugin_aggregator() : aggr_(0) {}

public:
    std::string name() const {
        return "aggregator";
    }

    float calculate(float x, float y) {
        aggr_ += x + y;
        return aggr_;
    }

    // Factory method
    static boost::shared_ptr<my_plugin_aggregator> create() {
        return boost::shared_ptr<my_plugin_aggregator>(
            new my_plugin_aggregator()
        );
    }
};


BOOST_DLL_ALIAS(
    my_namespace::my_plugin_aggregator::create, // <-- this function is exported with...
    create_plugin                               // <-- ...this alias name
)

} // namespace my_namespace
//]



