//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include <boost/beast/_experimental/unit_test/suite.hpp>

#ifdef BOOST_MSVC
#pragma warning(push)
#pragma warning(disable: 4459) // declaration hides global declaration
#endif

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace {

#include "websocket_common.ipp"

void
snippets()
{
    {
    //[code_websocket_1_1

        stream<tcp_stream> ws(ioc);
        net::ip::tcp::resolver resolver(ioc);

        // Connect the socket to the IP address returned from performing a name lookup
        get_lowest_layer(ws).connect(resolver.resolve("example.com", "ws"));

    //]
    }

    {
    //[code_websocket_1_2
    
        net::ip::tcp::acceptor acceptor(ioc);
        acceptor.bind(net::ip::tcp::endpoint(net::ip::tcp::v4(), 0));
        acceptor.listen();

        // The socket returned by accept() will be forwarded to the tcp_stream,
        // which uses it to perform a move-construction from the net::ip::tcp::socket.

        stream<tcp_stream> ws(acceptor.accept());

    //]
    }

    {
        net::ip::tcp::acceptor acceptor(ioc);
    //[code_websocket_1_3
    
        // The stream will use the strand for invoking all completion handlers
        stream<tcp_stream> ws(net::make_strand(ioc));

        // This overload of accept uses the socket provided for the new connection.
        // The function `tcp_stream::socket` provides access to the low-level socket
        // object contained in the tcp_stream.

        acceptor.accept(get_lowest_layer(ws).socket());

    //]
    }
}

struct doc_websocket_1_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,doc_websocket_1);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
