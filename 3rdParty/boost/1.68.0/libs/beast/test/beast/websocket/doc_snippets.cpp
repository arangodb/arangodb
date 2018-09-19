//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/core.hpp>
#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/use_future.hpp>
#include <algorithm>
#include <future>
#include <iostream>
#include <thread>

//[ws_snippet_1
#include <boost/beast/websocket.hpp>
using namespace boost::beast::websocket;
//]

using namespace boost::beast;

namespace doc_ws_snippets {

void fxx() {

boost::asio::io_service ios;
boost::asio::io_service::work work{ios};
std::thread t{[&](){ ios.run(); }};
error_code ec;
boost::asio::ip::tcp::socket sock{ios};

{
//[ws_snippet_2
    stream<boost::asio::ip::tcp::socket> ws{ios};
//]
}

{
//[ws_snippet_3
    stream<boost::asio::ip::tcp::socket> ws{std::move(sock)};
//]
}

{
//[ws_snippet_4
    stream<boost::asio::ip::tcp::socket&> ws{sock};
//]

//[ws_snippet_5
    ws.next_layer().shutdown(boost::asio::ip::tcp::socket::shutdown_send);
//]
}

{
//[ws_snippet_6
    std::string const host = "mywebapp.com";
    boost::asio::ip::tcp::resolver r{ios};
    stream<boost::asio::ip::tcp::socket> ws{ios};
    boost::asio::connect(ws.next_layer(), r.resolve({host, "ws"}));
//]
}

{
//[ws_snippet_7
    boost::asio::ip::tcp::acceptor acceptor{ios};
    stream<boost::asio::ip::tcp::socket> ws{acceptor.get_io_service()};
    acceptor.accept(ws.next_layer());
//]
}

{
    stream<boost::asio::ip::tcp::socket> ws{ios};
//[ws_snippet_8
    ws.handshake("localhost", "/");
//]

//[ws_snippet_9
    ws.handshake_ex("localhost", "/",
        [](request_type& m)
        {
            m.insert(http::field::sec_websocket_protocol, "xmpp;ws-chat");
        });
//]

//[ws_snippet_10
    response_type res;
    ws.handshake(res, "localhost", "/");
    if(! res.count(http::field::sec_websocket_protocol))
        throw std::invalid_argument("missing subprotocols");
//]

//[ws_snippet_11
    ws.accept();
//]

//[ws_snippet_12
    ws.accept_ex(
        [](response_type& m)
        {
            m.insert(http::field::server, "MyServer");
        });
//]
}

{
//[ws_snippet_13]
    // Buffer required for reading HTTP messages
    flat_buffer buffer;

    // Read the HTTP request ourselves
    http::request<http::string_body> req;
    http::read(sock, buffer, req);

    // See if its a WebSocket upgrade request
    if(websocket::is_upgrade(req))
    {
        // Construct the stream, transferring ownership of the socket
        stream<boost::asio::ip::tcp::socket> ws{std::move(sock)};

        // Accept the request from our message. Clients SHOULD NOT
        // begin sending WebSocket frames until the server has
        // provided a response, but just in case they did, we pass
        // any leftovers in the buffer to the accept function.
        //
        ws.accept(req, buffer.data());
    }
    else
    {
        // Its not a WebSocket upgrade, so
        // handle it like a normal HTTP request.
    }
//]
}

{
    stream<boost::asio::ip::tcp::socket> ws{ios};
//[ws_snippet_14
    // Read into our buffer until we reach the end of the HTTP request.
    // No parsing takes place here, we are just accumulating data.
    boost::asio::streambuf buffer;
    boost::asio::read_until(sock, buffer, "\r\n\r\n");

    // Now accept the connection, using the buffered data.
    ws.accept(buffer.data());
//]
}
{
    stream<boost::asio::ip::tcp::socket> ws{ios};
//[ws_snippet_15
    multi_buffer buffer;
    ws.read(buffer);

    ws.text(ws.got_text());
    ws.write(buffer.data());
    buffer.consume(buffer.size());
//]
}

{
    stream<boost::asio::ip::tcp::socket> ws{ios};
//[ws_snippet_16
    multi_buffer buffer;
    for(;;)
        if(ws.read_some(buffer, 0))
            break;
    ws.binary(ws.got_binary());
    consuming_buffers<multi_buffer::const_buffers_type> cb{buffer.data()};
    for(;;)
    {
        using boost::asio::buffer_size;
        if(buffer_size(cb) > 512)
        {
            ws.write_some(false, buffer_prefix(512, cb));
            cb.consume(512);
        }
        else
        {
            ws.write_some(true, cb);
            break;
        }
    }
//]
}

{
    stream<boost::asio::ip::tcp::socket> ws{ios};
//[ws_snippet_17
    auto cb =
        [](frame_type kind, string_view payload)
        {
            // Do something with the payload
            boost::ignore_unused(kind, payload);
        };
    ws.control_callback(cb);
//]

//[ws_snippet_18
    ws.close(close_code::normal);
//]

//[ws_snippet_19
    ws.auto_fragment(true);
    ws.write_buffer_size(16384);
//]

//[ws_snippet_20
    multi_buffer buffer;
    ws.async_read(buffer,
        [](error_code, std::size_t)
        {
            // Do something with the buffer
        });
//]
}

} // fxx()

// workaround for https://github.com/chriskohlhoff/asio/issues/112
#ifdef BOOST_MSVC
//[ws_snippet_21
void echo(stream<boost::asio::ip::tcp::socket>& ws,
    multi_buffer& buffer, boost::asio::yield_context yield)
{
    ws.async_read(buffer, yield);
    std::future<void> fut =
        ws.async_write(buffer.data(), boost::asio::use_future);
}
//]
#endif

} // doc_ws_snippets

//------------------------------------------------------------------------------

#if BOOST_BEAST_USE_OPENSSL

//[wss_snippet_1
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/ssl.hpp>
//]

namespace doc_wss_snippets {

void fxx() {

boost::asio::io_service ios;
boost::asio::io_service::work work{ios};
std::thread t{[&](){ ios.run(); }};
error_code ec;
boost::asio::ip::tcp::socket sock{ios};

{
//[wss_snippet_2
    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
    stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> wss{ios, ctx};
//]
}

{
//[wss_snippet_3
    boost::asio::ip::tcp::endpoint ep;
    boost::asio::ssl::context ctx{boost::asio::ssl::context::sslv23};
    stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>> ws{ios, ctx};

    // connect the underlying TCP/IP socket
    ws.next_layer().next_layer().connect(ep);

    // perform SSL handshake
    ws.next_layer().handshake(boost::asio::ssl::stream_base::client);

    // perform WebSocket handshake
    ws.handshake("localhost", "/");
//]
}

} // fxx()

} // doc_wss_snippets

#endif

