//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

//------------------------------------------------------------------------------
//
// Example: WebSocket client, coroutine
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <string>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Sends a WebSocket message and prints the response
void
do_session(
    std::string const& host,
    std::string const& port,
    std::string const& text,
    boost::asio::io_context& ioc,
    boost::asio::yield_context yield)
{
    boost::system::error_code ec;

    // These objects perform our I/O
    tcp::resolver resolver{ioc};
    websocket::stream<tcp::socket> ws{ioc};

    // Look up the domain name
    auto const results = resolver.async_resolve(host, port, yield[ec]);
    if(ec)
        return fail(ec, "resolve");

    // Make the connection on the IP address we get from a lookup
    boost::asio::async_connect(ws.next_layer(), results.begin(), results.end(), yield[ec]);
    if(ec)
        return fail(ec, "connect");

    // Perform the websocket handshake
    ws.async_handshake(host, "/", yield[ec]);
    if(ec)
        return fail(ec, "handshake");

    // Send the message
    ws.async_write(boost::asio::buffer(std::string(text)), yield[ec]);
    if(ec)
        return fail(ec, "write");

    // This buffer will hold the incoming message
    boost::beast::multi_buffer b;

    // Read a message into our buffer
    ws.async_read(b, yield[ec]);
    if(ec)
        return fail(ec, "read");

    // Close the WebSocket connection
    ws.async_close(websocket::close_code::normal, yield[ec]);
    if(ec)
        return fail(ec, "close");

    // If we get here then the connection is closed gracefully

    // The buffers() function helps print a ConstBufferSequence
    std::cout << boost::beast::buffers(b.data()) << std::endl;
}

//------------------------------------------------------------------------------

int main(int argc, char** argv)
{
    // Check command line arguments.
    if(argc != 4)
    {
        std::cerr <<
            "Usage: websocket-client-coro <host> <port> <text>\n" <<
            "Example:\n" <<
            "    websocket-client-coro echo.websocket.org 80 \"Hello, world!\"\n";
        return EXIT_FAILURE;
    }
    auto const host = argv[1];
    auto const port = argv[2];
    auto const text = argv[3];

    // The io_context is required for all I/O
    boost::asio::io_context ioc;

    // Launch the asynchronous operation
    boost::asio::spawn(ioc, std::bind(
        &do_session,
        std::string(host),
        std::string(port),
        std::string(text),
        std::ref(ioc),
        std::placeholders::_1));

    // Run the I/O service. The call will return when
    // the socket is closed.
    ioc.run();

    return EXIT_SUCCESS;
}
