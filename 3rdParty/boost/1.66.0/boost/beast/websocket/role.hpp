//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_ROLE_HPP
#define BOOST_BEAST_WEBSOCKET_ROLE_HPP

#include <boost/beast/core/detail/config.hpp>

namespace boost {
namespace beast {
namespace websocket {

/** The role of the websocket stream endpoint.

    Whether the endpoint is a client or server affects the
    behavior of the <em>Close the WebSocket Connection</em>
    operation described in rfc6455 section 7.1.1.
    The shutdown behavior depends on the type of the next
    layer template parameter used to construct the @ref stream.
    Other next layer types including user-defined types
    may implement different role-based behavior when
    performing the close operation.
    
    The default implementation for @ref stream when the next
    layer type is a `boost::asio::ip::tcp::socket` behaves
    as follows:

    @li In the client role, a TCP/IP shutdown is sent after
    reading all remaining data on the connection.

    @li In the server role, a TCP/IP shutdown is sent before
    reading all remaining data on the connection.

    When the next layer type is a `boost::asio::ssl::stream`,
    the connection is closed by performing the SSL closing
    handshake corresponding to the role type, client or server.

    @see https://tools.ietf.org/html/rfc6455#section-7.1.1
*/
enum class role_type
{
    /// The stream is operating as a client.
    client,

    /// The stream is operating as a server.
    server
};

} // websocket
} // beast
} // boost

#endif
