//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_TEARDOWN_HPP
#define BOOST_BEAST_WEBSOCKET_TEARDOWN_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/websocket/role.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <type_traits>

namespace boost {
namespace beast {
namespace websocket {

/** Tear down a connection.

    This tears down a connection. The implementation will call
    the overload of this function based on the `Socket` parameter
    used to consruct the socket. When `Socket` is a user defined
    type, and not a `boost::asio::ip::tcp::socket` or any
    `boost::asio::ssl::stream`, callers are responsible for
    providing a suitable overload of this function.

    @param role The role of the local endpoint

    @param socket The socket to tear down.

    @param ec Set to the error if any occurred.
*/
template<class Socket>
void
teardown(
    role_type role,
    Socket& socket,
    error_code& ec)
{
    boost::ignore_unused(role, socket, ec);
/*
    If you are trying to use OpenSSL and this goes off, you need to
    add an include for <boost/beast/websocket/ssl.hpp>.

    If you are creating an instance of beast::websocket::stream with your
    own user defined type, you must provide an overload of teardown with
    the corresponding signature (including the role_type).
*/
    static_assert(sizeof(Socket)==-1,
        "Unknown Socket type in teardown.");
}

/** Start tearing down a connection.

    This begins tearing down a connection asynchronously.
    The implementation will call the overload of this function
    based on the `Socket` parameter used to consruct the socket.
    When `Stream` is a user defined type, and not a
    `boost::asio::ip::tcp::socket` or any `boost::asio::ssl::stream`,
    callers are responsible for providing a suitable overload
    of this function.

    @param role The role of the local endpoint

    @param socket The socket to tear down.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    );
    @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using boost::asio::io_context::post().

*/
template<
    class Socket,
    class TeardownHandler>
void
async_teardown(
    role_type role,
    Socket& socket,
    TeardownHandler&& handler)
{
    boost::ignore_unused(role, socket, handler);
/*
    If you are trying to use OpenSSL and this goes off, you need to
    add an include for <boost/beast/websocket/ssl.hpp>.

    If you are creating an instance of beast::websocket::stream with your
    own user defined type, you must provide an overload of teardown with
    the corresponding signature (including the role_type).
*/
    static_assert(sizeof(Socket)==-1,
        "Unknown Socket type in async_teardown.");
}

} // websocket

//------------------------------------------------------------------------------

namespace websocket {

/** Tear down a `boost::asio::ip::tcp::socket`.

    This tears down a connection. The implementation will call
    the overload of this function based on the `Stream` parameter
    used to consruct the socket. When `Stream` is a user defined
    type, and not a `boost::asio::ip::tcp::socket` or any
    `boost::asio::ssl::stream`, callers are responsible for
    providing a suitable overload of this function.

    @param role The role of the local endpoint

    @param socket The socket to tear down.

    @param ec Set to the error if any occurred.
*/
void
teardown(
    role_type role,
    boost::asio::ip::tcp::socket& socket,
    error_code& ec);

/** Start tearing down a `boost::asio::ip::tcp::socket`.

    This begins tearing down a connection asynchronously.
    The implementation will call the overload of this function
    based on the `Stream` parameter used to consruct the socket.
    When `Stream` is a user defined type, and not a
    `boost::asio::ip::tcp::socket` or any `boost::asio::ssl::stream`,
    callers are responsible for providing a suitable overload
    of this function.

    @param role The role of the local endpoint

    @param socket The socket to tear down.

    @param handler Invoked when the operation completes.
    The handler may be moved or copied as needed.
    The equivalent function signature of the handler must be:
    @code void handler(
        error_code const& error // result of operation
    );
    @endcode
    Regardless of whether the asynchronous operation completes
    immediately or not, the handler will not be invoked from within
    this function. Invocation of the handler will be performed in a
    manner equivalent to using boost::asio::io_context::post().

*/
template<class TeardownHandler>
void
async_teardown(
    role_type role,
    boost::asio::ip::tcp::socket& socket,
    TeardownHandler&& handler);

} // websocket
} // beast
} // boost

#include <boost/beast/websocket/impl/teardown.ipp>

#endif
