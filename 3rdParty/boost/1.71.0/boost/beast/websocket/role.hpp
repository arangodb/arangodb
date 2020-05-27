//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_ROLE_HPP
#define BOOST_BEAST_WEBSOCKET_ROLE_HPP

#include <boost/beast/core/detail/config.hpp>

#ifndef BOOST_BEAST_ALLOW_DEPRECATED

#error This file is deprecated interface, #define BOOST_BEAST_ALLOW_DEPRECATED to allow it

#else

#include <boost/beast/core/role.hpp>

namespace boost {
namespace beast {
namespace websocket {

using role_type = beast::role_type;

} // websocket
} // beast
} // boost

#endif

#endif
