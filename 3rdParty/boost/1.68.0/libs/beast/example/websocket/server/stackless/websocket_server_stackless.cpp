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
// Example: WebSocket server, stackless coroutine
//
//------------------------------------------------------------------------------

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using tcp = boost::asio::ip::tcp;               // from <boost/asio/ip/tcp.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// Echoes back all received WebSocket messages
class session
    : public boost::asio::coroutine
    , public std::enable_shared_from_this<session>
{
    websocket::stream<tcp::socket> ws_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::beast::multi_buffer buffer_;

public:
    // Take ownership of the socket
    explicit
    session(tcp::socket socket)
        : ws_(std::move(socket))
        , strand_(ws_.get_executor())
    {
    }

    // Start the asynchronous operation
    void
    run()
    {
        loop({}, 0);
    }

#include <boost/asio/yield.hpp>
    void
    loop(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);
        reenter(*this)
        {
            // Accept the websocket handshake
            yield ws_.async_accept(
                boost::asio::bind_executor(
                    strand_,
                    std::bind(
                        &session::loop,
                        shared_from_this(),
                        std::placeholders::_1,
                        0)));
            if(ec)
                return fail(ec, "accept");

            for(;;)
            {
                // Read a message into our buffer
                yield ws_.async_read(
                    buffer_,
                    boost::asio::bind_executor(
                        strand_,
                        std::bind(
                            &session::loop,
                            shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2)));
                if(ec == websocket::error::closed)
                {
                    // This indicates that the session was closed
                    return;
                }
                if(ec)
                    fail(ec, "read");

                // Echo the message
                ws_.text(ws_.got_text());
                yield ws_.async_write(
                    buffer_.data(),
                    boost::asio::bind_executor(
                        strand_,
                        std::bind(
                            &session::loop,
                            shared_from_this(),
                            std::placeholders::_1,
                            std::placeholders::_2)));
                if(ec)
                    return fail(ec, "write");

                // Clear the buffer
                buffer_.consume(buffer_.size());
            }
        }
    }
#include <boost/asio/unyield.hpp>
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class listener
    : public boost::asio::coroutine
    , public std::enable_shared_from_this<listener>
{
    tcp::acceptor acceptor_;
    tcp::socket socket_;

public:
    listener(
        boost::asio::io_context& ioc,
        tcp::endpoint endpoint)
        : acceptor_(ioc)
        , socket_(ioc)
    {
        boost::system::error_code ec;

        // Open the acceptor
        acceptor_.open(endpoint.protocol(), ec);
        if(ec)
        {
            fail(ec, "open");
            return;
        }

        // Allow address reuse
        acceptor_.set_option(boost::asio::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }

        // Bind to the server address
        acceptor_.bind(endpoint, ec);
        if(ec)
        {
            fail(ec, "bind");
            return;
        }

        // Start listening for connections
        acceptor_.listen(
            boost::asio::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }

    // Start accepting incoming connections
    void
    run()
    {
        if(! acceptor_.is_open())
            return;
        loop();
    }

#include <boost/asio/yield.hpp>
    void
    loop(boost::system::error_code ec = {})
    {
        reenter(*this)
        {
            for(;;)
            {
                yield acceptor_.async_accept(
                    socket_,
                    std::bind(
                        &listener::loop,
                        shared_from_this(),
                        std::placeholders::_1));
                if(ec)
                {
                    fail(ec, "accept");
                }
                else
                {
                    // Create the session and run it
                    std::make_shared<session>(std::move(socket_))->run();
                }
            }
        }
    }
#include <boost/asio/unyield.hpp>
};

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr <<
            "Usage: websocket-server-stackless <address> <port> <threads>\n" <<
            "Example:\n" <<
            "    websocket-server-stackless 0.0.0.0 8080 1\n";
        return EXIT_FAILURE;
    }
    auto const address = boost::asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};

    // Create and launch a listening port
    std::make_shared<listener>(ioc, tcp::endpoint{address, port})->run();

    // Run the I/O service on the requested number of threads
    std::vector<std::thread> v;
    v.reserve(threads - 1);
    for(auto i = threads - 1; i > 0; --i)
        v.emplace_back(
        [&ioc]
        {
            ioc.run();
        });
    ioc.run();

    return EXIT_SUCCESS;
}
