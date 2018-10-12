//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/type_traits.hpp>

#include <boost/beast/http/empty_body.hpp>
#include <string>

namespace boost {
namespace beast {
namespace http {

BOOST_STATIC_ASSERT(! is_body_writer<int>::value);

BOOST_STATIC_ASSERT(is_body_writer<empty_body>::value);

BOOST_STATIC_ASSERT(! is_body_reader<std::string>::value);

namespace {

struct not_fields {};

} // (anonymous)

BOOST_STATIC_ASSERT(! is_fields<not_fields>::value);

} // http
} // beast
} // boost
