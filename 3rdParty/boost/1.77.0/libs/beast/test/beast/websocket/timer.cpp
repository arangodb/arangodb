
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
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/detached.hpp>

namespace boost {
namespace beast {
namespace websocket {

struct timer_test : unit_test::suite
{
    using tcp = boost::asio::ip::tcp;

    void
    testIdlePing()
    {
        net::io_context ioc;

        // idle ping, no timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_accept(test::success_handler());
            ws2.async_handshake("test", "/", test::success_handler());
            test::run(ioc);

            ws2.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(100),
                true});
            flat_buffer b1;
            flat_buffer b2;
            bool received = false;
            ws1.control_callback(
                [&received](frame_type ft, string_view)
                {
                    received = true;
                    BEAST_EXPECT(ft == frame_type::ping);
                });
            ws1.async_read(b1, test::fail_handler(
                net::error::operation_aborted));
            ws2.async_read(b2, test::fail_handler(
                net::error::operation_aborted));
            test::run_for(ioc, std::chrono::milliseconds(500));
            BEAST_EXPECT(received);
        }

        test::run(ioc);

        // idle ping, timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());
            ws1.async_accept(test::success_handler());
            ws2.async_handshake("test", "/", test::success_handler());
            test::run(ioc);

            ws2.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::milliseconds(50),
                true});
            flat_buffer b;
            ws2.async_read(b,
                test::fail_handler(beast::error::timeout));
            test::run(ioc);
        }

        test::run(ioc);
    }

    // https://github.com/boostorg/beast/issues/1729
    void
    testIssue1729()
    {
        {
            net::io_context ioc;
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(websocket::stream_base::timeout{
                std::chrono::milliseconds(50),
                websocket::stream_base::none(),
                false});

            ws1.async_accept(net::detached);
            ws2.async_handshake(
                "localhost", "/", net::detached);
            ioc.run();
            ioc.restart();

            flat_buffer b;
            error_code ec2;
            ws1.async_close({},
                [&ec2](error_code ec)
                {
                    ec2 = ec;
                });
            ioc.run();
            BEAST_EXPECTS(
                ec2 == beast::error::timeout ||
                    ec2 == net::error::operation_aborted,
                ec2.message());
        }
        {
            net::io_context ioc;
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(websocket::stream_base::timeout{
                std::chrono::milliseconds(50),
                websocket::stream_base::none(),
                false});

            ws1.async_accept(net::detached);
            ws2.async_handshake(
                "localhost", "/", net::detached);
            ioc.run();
            ioc.restart();

            flat_buffer b;
            error_code ec1, ec2;
            ws1.async_read(b,
                [&ec1](error_code ec, std::size_t)
                {
                    ec1 = ec;
                });
            ws1.async_close({},
                [&ec2](error_code ec)
                {
                    ec2 = ec;
                });
            ioc.run();
            BEAST_EXPECT(
                ec1 == beast::error::timeout ||
                ec2 == beast::error::timeout);
        }
    }

    // https://github.com/boostorg/beast/issues/1729#issuecomment-540481056
    void
    testCloseWhileRead()
    {
        net::io_context ioc;
        stream<tcp::socket> ws1(ioc);
        stream<tcp::socket> ws2(ioc);
        test::connect(ws1.next_layer(), ws2.next_layer());

        ws1.set_option(websocket::stream_base::timeout{
            std::chrono::milliseconds(50),
            websocket::stream_base::none(),
            false});

        ws1.async_accept(net::detached);
        ws2.async_handshake(
            "localhost", "/", net::detached);
        ioc.run();
        ioc.restart();

        flat_buffer b;
        ws1.async_read(b, test::fail_handler(
            beast::error::timeout));
        ws1.async_close({}, test::fail_handler(
            net::error::operation_aborted));
        ioc.run();
    }

    void
    run() override
    {
        testIssue1729();
        testIdlePing();
        testCloseWhileRead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,timer);

} // websocket
} // beast
} // boost
