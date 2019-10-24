//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/test/tcp.hpp>
#include <boost/beast/core/flat_buffer.hpp>

namespace boost {
namespace beast {
namespace websocket {

class read1_test : public unit_test::suite
{
public:
    void
    testTimeout()
    {
        using tcp = net::ip::tcp;

        net::io_context ioc;

        // success

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            ws2.async_read(b, test::success_handler());
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            ws2.async_read(b, test::success_handler());
            test::run(ioc);
        }

        // success, timeout enabled

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::success_handler());
            ws2.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::success_handler());
            ws2.async_write(net::const_buffer("Hello, world!", 13),
                test::success_handler());
            test::run(ioc);
        }

        // timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::fail_handler(
                beast::error::timeout));
            test::run(ioc);
        }

        {
            stream<test::stream> ws1(ioc);
            stream<test::stream> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run(ioc);

            flat_buffer b;
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                false});
            ws1.async_read(b, test::fail_handler(
                beast::error::timeout));
            test::run(ioc);
        }
    }

    void
    run() override
    {
        testTimeout();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read1);

} // websocket
} // beast
} // boost
