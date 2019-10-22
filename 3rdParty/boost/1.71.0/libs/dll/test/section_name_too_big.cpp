// Copyright 2011-2012 Renato Tegon Forti
// Copyright 2014 Renato Tegon Forti, Antony Polukhin.
// Copyright 2015-2019 Antony Polukhin.
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// For more information, see http://www.boost.org

#define BOOST_DLL_FORCE_ALIAS_INSTANTIATION
#include <boost/dll/config.hpp>
#include <boost/dll/alias.hpp>

int integer_g = 100;
BOOST_DLL_ALIAS_SECTIONED(integer_g, integer_g_aliased, "long_section_name")

