//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_CORE_DETAIL_CONFIG_HPP
#define BOOST_BEAST_CORE_DETAIL_CONFIG_HPP

#include <boost/config.hpp>
#include <boost/version.hpp>

// Available to every header
#include <boost/config.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/static_assert.hpp>

/*
    _MSC_VER and _MSC_FULL_VER by version:

    14.0 (2015)             1900        190023026
    14.0 (2015 Update 1)    1900        190023506
    14.0 (2015 Update 2)    1900        190023918
    14.0 (2015 Update 3)    1900        190024210
*/

#if defined(BOOST_MSVC)
# if BOOST_MSVC_FULL_VER < 190024210
#  error Beast requires C++11: Visual Studio 2015 Update 3 or later needed
# endif

#elif defined(BOOST_GCC)
# if(BOOST_GCC < 40801)
#  error Beast requires C++11: gcc version 4.8 or later needed
# endif

#else
# if \
    defined(BOOST_NO_CXX11_DECLTYPE) || \
    defined(BOOST_NO_CXX11_HDR_TUPLE) || \
    defined(BOOST_NO_CXX11_TEMPLATE_ALIASES) || \
    defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
#  error Beast requires C++11: a conforming compiler is needed
# endif

#endif

#define BOOST_BEAST_DEPRECATION_STRING \
    "This is a deprecated interface, #define BOOST_BEAST_ALLOW_DEPRECATED to allow it"

#endif
