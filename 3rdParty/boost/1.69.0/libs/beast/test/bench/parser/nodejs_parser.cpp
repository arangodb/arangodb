//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif

#include <boost/config.hpp>

#ifdef BOOST_MSVC
# pragma warning (push)
# pragma warning (disable: 4127) // conditional expression is constant
# pragma warning (disable: 4244) // integer conversion, possible loss of data
#endif

#include "nodejs-parser/http_parser.c"

#ifdef BOOST_MSVC
# pragma warning (pop)
#endif

#if defined(__GNUC__) && (__GNUC__ >= 7)
#pragma GCC diagnostic pop
#endif
