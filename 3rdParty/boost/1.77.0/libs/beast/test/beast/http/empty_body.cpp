//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/empty_body.hpp>

namespace boost {
namespace beast {
namespace http {

BOOST_STATIC_ASSERT(is_body<empty_body>::value);
BOOST_STATIC_ASSERT(is_body_writer<empty_body>::value);
BOOST_STATIC_ASSERT(is_body_reader<empty_body>::value);

} // http
} // beast
} // boost
