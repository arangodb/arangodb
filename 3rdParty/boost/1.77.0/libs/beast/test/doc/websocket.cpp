//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/config.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4459) // declaration hides global declaration
#endif

#include <boost/beast/_experimental/unit_test/suite.hpp>

//[code_websocket_1a

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

//]

namespace {

#include "websocket_common.ipp"

void
snippets()
{
    {
    //[code_websocket_1f

        // This newly constructed WebSocket stream will use the specified
        // I/O context and have support for the permessage-deflate extension.

        stream<tcp_stream> ws(ioc);

    //]
    }

    {
    //[code_websocket_2f

        // The `tcp_stream` will be constructed with a new
        // strand which uses the specified I/O context.

        stream<tcp_stream> ws(net::make_strand(ioc));

    //]
    }

    {
    //[code_websocket_3f

        // Ownership of the `tcp_stream` is transferred to the websocket stream

        stream<tcp_stream> ws(std::move(sock));

    //]
    }
    
    {
        stream<tcp_stream> ws(ioc);
    //[code_websocket_4f

        // Calls `close` on the underlying `beast::tcp_stream`
        ws.next_layer().close();

    //]
    }

    {
    //[code_websocket_5f

        // The WebSocket stream will use SSL and a new strand
        stream<ssl_stream<tcp_stream>> wss(net::make_strand(ioc), ctx);

    //]

    //[code_websocket_6f

        // Perform the SSL handshake in the client role
        wss.next_layer().handshake(net::ssl::stream_base::client);

    //]

    //[code_websocket_7f

        // Cancel all pending I/O on the underlying `tcp_stream`
        get_lowest_layer(wss).cancel();

    //]
    }
}

struct doc_websocket_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,doc_websocket);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
