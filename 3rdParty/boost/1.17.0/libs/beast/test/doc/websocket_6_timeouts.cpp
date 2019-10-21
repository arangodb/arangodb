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

#include <iostream>

namespace {

#include "websocket_common.ipp"

void
snippets()
{
    {
  
    stream<tcp_stream> ws(ioc);

    {
    //[code_websocket_6_1

        // Apply suggested timeout options for the server role to the stream
        ws.set_option(stream_base::timeout::suggested(role_type::server));

    //]
    }

    {
    //[code_websocket_6_2

        stream_base::timeout opt{
            std::chrono::seconds(30),   // handshake timeout
            stream_base::none(),        // idle timeout
            false
        };

        // Set the timeout options on the stream.
        ws.set_option(opt);

    //]
    }

    {
        flat_buffer b;
    //[code_websocket_6_3

        ws.async_read(b,
            [](error_code ec, std::size_t)
            {
                if(ec == beast::error::timeout)
                    std::cerr << "timeout, connection closed!";
            });
    //]
    }

    }

    {
    //[code_websocket_6_4

        // Disable any timeouts on the tcp_stream
        sock.expires_never();

        // Construct the websocket stream, taking ownership of the existing tcp_stream
        stream<tcp_stream> ws(std::move(sock));

    //]
    }
}

struct websocket_6_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_6);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
