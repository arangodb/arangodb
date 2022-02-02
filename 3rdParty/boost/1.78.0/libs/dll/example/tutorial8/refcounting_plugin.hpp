// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2021 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

//[plugcpp_my_plugin_refcounting_hpp
#include "refcounting_api.hpp"
#include <boost/dll/alias.hpp> // for BOOST_DLL_ALIAS

my_refcounting_api* create(); // defined in plugin
BOOST_DLL_ALIAS(create, create_refc_plugin)
//]



