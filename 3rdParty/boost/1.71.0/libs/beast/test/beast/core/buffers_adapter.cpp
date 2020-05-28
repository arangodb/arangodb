//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_ALLOW_DEPRECATED
#define BOOST_BEAST_ALLOW_DEPRECATED
#endif

// Test that header file is self-contained.
#include <boost/beast/core/buffers_adapter.hpp>

namespace boost {
namespace beast {

BOOST_STATIC_ASSERT(sizeof(buffers_adapter<net::mutable_buffer>) > 0);

} // beast
} // boost
