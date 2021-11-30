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

#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/spawn.hpp>

namespace {

#include "websocket_common.ipp"

void
snippets()
{
    stream<tcp_stream> ws(ioc);

    {
    //[code_websocket_8_1

        flat_buffer buffer;

        ws.async_read(buffer,
            [](error_code, std::size_t)
            {
                // Do something with the buffer
            });

    //]
    }

    multi_buffer b;

    {
    //[code_websocket_8_2

        ws.async_read(b, [](error_code, std::size_t){});
        ws.async_read(b, [](error_code, std::size_t){});

    //]
    }

    {
    //[code_websocket_8_3

        ws.async_read(b, [](error_code, std::size_t){});
        ws.async_write(b.data(), [](error_code, std::size_t){});
        ws.async_ping({}, [](error_code){});
        ws.async_close({}, [](error_code){});

    //]
    }
}

// workaround for https://github.com/chriskohlhoff/asio/issues/112
//#ifdef BOOST_MSVC
//[code_websocket_8_1f

void echo(stream<tcp_stream>& ws,
    multi_buffer& buffer, net::yield_context yield)
{
    ws.async_read(buffer, yield);
    std::future<std::size_t> fut =
        ws.async_write(buffer.data(), net::use_future);
}

//]
//#endif

struct websocket_8_test
    : public boost::beast::unit_test::suite
{
    void
    run() override
    {
        BEAST_EXPECT(&snippets);
        BEAST_EXPECT(&echo);
    }
};

BEAST_DEFINE_TESTSUITE(beast,doc,websocket_8);

} // (anon)

#ifdef BOOST_MSVC
#pragma warning(pop)
#endif
