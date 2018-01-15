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
// Example: WebSocket server, fast
//
//------------------------------------------------------------------------------

/*  This server contains the following ports:

    Synchronous     <base port + 0>
    Asynchronous    <base port + 1>
    Coroutine       <base port + 2>

    This program is optimized for the Autobahn|Testsuite
    benchmarking and WebSocket compliants testing program.

    See:
    https://github.com/crossbario/autobahn-testsuite
*/

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/spawn.hpp>
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
namespace http = boost::beast::http;            // from <boost/beast/http.hpp>
namespace websocket = boost::beast::websocket;  // from <boost/beast/websocket.hpp>

//------------------------------------------------------------------------------

// Report a failure
void
fail(boost::system::error_code ec, char const* what)
{
    std::cerr << (std::string(what) + ": " + ec.message() + "\n");
}

// Adjust settings on the stream
template<class NextLayer>
void
setup_stream(websocket::stream<NextLayer>& ws)
{
    // These values are tuned for Autobahn|Testsuite, and
    // should also be generally helpful for increased performance.

    websocket::permessage_deflate pmd;
    pmd.client_enable = true;
    pmd.server_enable = true;
    pmd.compLevel = 3;
    ws.set_option(pmd);

    ws.auto_fragment(false);

    // Autobahn|Testsuite needs this
    ws.read_message_max(64 * 1024 * 1024);
}

//------------------------------------------------------------------------------

void
do_sync_session(tcp::socket& socket)
{
    boost::system::error_code ec;

    websocket::stream<tcp::socket> ws{std::move(socket)};
    setup_stream(ws);

    ws.accept_ex(
        [](websocket::response_type& res)
        {
            res.set(http::field::server,
                "Boost.Beast/" + std::to_string(BOOST_BEAST_VERSION) + "-Sync");
        },
        ec);
    if(ec)
        return fail(ec, "accept");

    for(;;)
    {
        boost::beast::multi_buffer buffer;
        
        ws.read(buffer, ec);
        if(ec == websocket::error::closed)
            break;
        if(ec)
            return fail(ec, "read");
        ws.text(ws.got_text());
        ws.write(buffer.data(), ec);
        if(ec)
            return fail(ec, "write");
    }
}

void
do_sync_listen(
    boost::asio::io_context& ioc,
    tcp::endpoint endpoint)
{
    boost::system::error_code ec;
    tcp::acceptor acceptor{ioc, endpoint};
    for(;;)
    {
        tcp::socket socket{ioc};

        acceptor.accept(socket, ec);
        if(ec)
            return fail(ec, "accept");

        std::thread{std::bind(
            &do_sync_session,
            std::move(socket))}.detach();
    }
}

//------------------------------------------------------------------------------

// Echoes back all received WebSocket messages
class async_session : public std::enable_shared_from_this<async_session>
{
    websocket::stream<tcp::socket> ws_;
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    boost::beast::multi_buffer buffer_;

public:
    // Take ownership of the socket
    explicit
    async_session(tcp::socket socket)
        : ws_(std::move(socket))
        , strand_(ws_.get_executor())
    {
        setup_stream(ws_);
    }

    // Start the asynchronous operation
    void
    run()
    {
        // Accept the websocket handshake
        ws_.async_accept_ex(
            [](websocket::response_type& res)
            {
                res.set(http::field::server,
                    "Boost.Beast/" + std::to_string(BOOST_BEAST_VERSION) + "-Async");
            },
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &async_session::on_accept,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if(ec)
            return fail(ec, "accept");

        // Read a message
        do_read();
    }

    void
    do_read()
    {
        // Read a message into our buffer
        ws_.async_read(
            buffer_,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &async_session::on_read,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_read(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        // This indicates that the async_session was closed
        if(ec == websocket::error::closed)
            return;

        if(ec)
            fail(ec, "read");

        // Echo the message
        ws_.text(ws_.got_text());
        ws_.async_write(
            buffer_.data(),
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &async_session::on_write,
                    shared_from_this(),
                    std::placeholders::_1,
                    std::placeholders::_2)));
    }

    void
    on_write(
        boost::system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Clear the buffer
        buffer_.consume(buffer_.size());

        // Do another read
        do_read();
    }
};

// Accepts incoming connections and launches the sessions
class async_listener : public std::enable_shared_from_this<async_listener>
{
    boost::asio::strand<
        boost::asio::io_context::executor_type> strand_;
    tcp::acceptor acceptor_;
    tcp::socket socket_;

public:
    async_listener(
        boost::asio::io_context& ioc,
        tcp::endpoint endpoint)
        : strand_(ioc.get_executor())
        , acceptor_(ioc)
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
        do_accept();
    }

    void
    do_accept()
    {
        acceptor_.async_accept(
            socket_,
            boost::asio::bind_executor(
                strand_,
                std::bind(
                    &async_listener::on_accept,
                    shared_from_this(),
                    std::placeholders::_1)));
    }

    void
    on_accept(boost::system::error_code ec)
    {
        if(ec)
        {
            fail(ec, "accept");
        }
        else
        {
            // Create the async_session and run it
            std::make_shared<async_session>(std::move(socket_))->run();
        }

        // Accept another connection
        do_accept();
    }
};

//------------------------------------------------------------------------------

void
do_coro_session(tcp::socket& socket, boost::asio::yield_context yield)
{
    boost::system::error_code ec;

    websocket::stream<tcp::socket> ws{std::move(socket)};
    setup_stream(ws);

    ws.async_accept_ex(
        [&](websocket::response_type& res)
        {
            res.set(http::field::server,
                "Boost.Beast/" + std::to_string(BOOST_BEAST_VERSION) + "-Coro");
        },
        yield[ec]);
    if(ec)
        return fail(ec, "accept");

    for(;;)
    {
        boost::beast::multi_buffer buffer;

        ws.async_read(buffer, yield[ec]);
        if(ec == websocket::error::closed)
            break;
        if(ec)
            return fail(ec, "read");

        ws.text(ws.got_text());
        ws.async_write(buffer.data(), yield[ec]);
        if(ec)
            return fail(ec, "write");
    }
}

void
do_coro_listen(
    boost::asio::io_context& ioc,
    tcp::endpoint endpoint,
    boost::asio::yield_context yield)
{
    boost::system::error_code ec;

    tcp::acceptor acceptor(ioc);
    acceptor.open(endpoint.protocol(), ec);
    if(ec)
        return fail(ec, "open");

    acceptor.bind(endpoint, ec);
    if(ec)
        return fail(ec, "bind");

    acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
    if(ec)
        return fail(ec, "listen");

    for(;;)
    {
        tcp::socket socket(ioc);

        acceptor.async_accept(socket, yield[ec]);
        if(ec)
        {
            fail(ec, "accept");
            continue;
        }

        boost::asio::spawn(
            acceptor.get_executor().context(),
            std::bind(
                &do_coro_session,
                std::move(socket),
                std::placeholders::_1));
    }
}

//------------------------------------------------------------------------------

int main(int argc, char* argv[])
{
    // Check command line arguments.
    if (argc != 4)
    {
        std::cerr <<
            "Usage: websocket-server-fast <address> <starting-port> <threads>\n" <<
            "Example:\n"
            "    websocket-server-fast 0.0.0.0 8080 1\n"
            "  Connect to:\n"
            "    starting-port+0 for synchronous,\n"
            "    starting-port+1 for asynchronous,\n"
            "    starting-port+2 for coroutine.\n";
        return EXIT_FAILURE;
    }
    auto const address = boost::asio::ip::make_address(argv[1]);
    auto const port = static_cast<unsigned short>(std::atoi(argv[2]));
    auto const threads = std::max<int>(1, std::atoi(argv[3]));

    // The io_context is required for all I/O
    boost::asio::io_context ioc{threads};

    // Create sync port
    std::thread(std::bind(
        &do_sync_listen,
        std::ref(ioc),
        tcp::endpoint{
            address,
            static_cast<unsigned short>(port + 0u)}
                )).detach();

    // Create async port
    std::make_shared<async_listener>(
        ioc,
        tcp::endpoint{
            address,
            static_cast<unsigned short>(port + 1u)})->run();

    // Create coro port
    boost::asio::spawn(ioc,
        std::bind(
            &do_coro_listen,
            std::ref(ioc),
            tcp::endpoint{
                address,
                static_cast<unsigned short>(port + 2u)},
            std::placeholders::_1));

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
