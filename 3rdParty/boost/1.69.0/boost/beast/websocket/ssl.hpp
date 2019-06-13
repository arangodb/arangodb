//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_SSL_HPP
#define BOOST_BEAST_WEBSOCKET_SSL_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/websocket/teardown.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

namespace boost {
namespace beast {
namespace websocket {

/** Tear down a `boost::asio::ssl::stream`.

    This tears down a connection. The implementation will call
    the overload of this function based on the `Stream` parameter
    used to consruct the socket. When `Stream` is a user defined
    type, and not a `boost::asio::ip::tcp::socket` or any
    `boost::asio::ssl::stream`, callers are responsible for
    providing a suitable overload of this function.

    @param role The role of the local endpoint

    @param stream The stream to tear down.

    @param ec Set to the error if any occurred.
*/
template<class SyncStream>
void
teardown(
    role_type role,
    boost::asio::ssl::stream<SyncStream>& stream,
    error_code& ec);

/** Start tearing down a `boost::asio::ssl::stream`.

    This begins tearing down a connection asynchronously.
    The implementation will call the overload of this function
    based on the `Stream` parameter used to consruct the socket.
    When `Stream` is a user defined type, and not a
    `boost::asio::ip::tcp::socket` or any `boost::asio::ssl::stream`,
    callers are responsible for providing a suitable overload
    of this function.

    @param role The role of the local endpoint

    @param stream The stream to tear down.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    ); @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using boost::asio::io_context::post().

*/
template<class AsyncStream, class TeardownHandler>
inline
void
async_teardown(
    role_type role,
    boost::asio::ssl::stream<AsyncStream>& stream,
    TeardownHandler&& handler);

} // websocket
} // beast
} // boost

#include <boost/beast/websocket/impl/ssl.ipp>

#endif
