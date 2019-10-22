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

#include <boost/beast/_experimental/test/tcp.hpp>

#include "test.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

namespace boost {
namespace beast {
namespace websocket {

class ping_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestPing(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // ping
        doTest(pmd, [&](ws_type& ws)
        {
            w.ping(ws, {});
        });

        // pong
        doTest(pmd, [&](ws_type& ws)
        {
            w.pong(ws, {});
        });

        // ping, already closed
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.close({});
            try
            {
                w.ping(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == net::error::operation_aborted,
                    se.code().message());
            }
        }

        // pong, already closed
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.close({});
            try
            {
                w.pong(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == net::error::operation_aborted,
                    se.code().message());
            }
        }
    }

    void
    testPing()
    {
        doTestPing(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestPing(AsyncClient{yield});
        });
    }

    void
    testSuspend()
    {
        // suspend on write
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            ws.async_write(sbuf("Hello, world"),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 12);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on close
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            ws.async_close({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read ping + message
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // add a ping and message to the input
            ws.next_layer().append(string_view{
                "\x89\x00" "\x81\x01*", 5});
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            while(! ws.impl_->wr_block.is_locked())
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read bad message
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // add an invalid frame to the input
            ws.next_layer().append(string_view{
                "\x09\x00", 2});

            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    if(ec != error::bad_control_fragment)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            while(! ws.impl_->wr_block.is_locked())
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read close #1
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // add a close frame to the input
            ws.next_layer().append(string_view{
                "\x88\x00", 2});
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    if(ec != error::closed)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            while(! ws.impl_->wr_block.is_locked())
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read close #2
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // Cause close to be received
            es.async_close();
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    if(ec != error::closed)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            while(! ws.impl_->wr_block.is_locked())
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_ping({},
                [&](error_code ec)
                {
                    ++count;
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // don't ping on close
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            error_code ec;
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            ws.async_write(sbuf("*"),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 1);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ws.async_close({},
                [&](error_code)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // suspend idle ping
        {
            using socket_type =
                net::basic_stream_socket<
                    net::ip::tcp,
                    net::executor>;
            net::io_context ioc;
            stream<socket_type> ws1(ioc);
            stream<socket_type> ws2(ioc);
            ws1.set_option(stream_base::timeout{
                stream_base::none(),
                std::chrono::seconds(0),
                true});
            test::connect(
                ws1.next_layer(),
                ws2.next_layer());
            ws1.async_handshake("localhost", "/",
                [](error_code){});
            ws2.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            flat_buffer b1;
            auto mb = b1.prepare(65536);
            std::memset(mb.data(), 0, mb.size());
            b1.commit(65536);
            ws1.async_write(b1.data(),
                [&](error_code, std::size_t){});
            BEAST_EXPECT(
                ws1.impl_->wr_block.is_locked());
            ws1.async_read_some(net::mutable_buffer{},
                [&](error_code, std::size_t){});
            ioc.run();
            ioc.restart();
            flat_buffer b2;
            ws2.async_read(b2,
                [&](error_code, std::size_t){});
            ioc.run();
        }
        //);

        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");

            // Cause close to be received
            es.async_close();
            
            multi_buffer b;
            std::size_t count = 0;
            // Read a close frame.
            // Sends a close frame, blocking writes.
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    // Read should complete with error::closed
                    ++count;
                    BEAST_EXPECTS(ec == error::closed,
                        ec.message());
                });
            if(! BEAST_EXPECT(run_until(ioc, 100,
                    [&]{ return ws.impl_->wr_close; })))
                return;
            // Try to ping
            ws.async_ping("payload",
                [&](error_code ec)
                {
                    // Pings after a close are aborted
                    ++count;
                    BEAST_EXPECTS(ec == net::
                        error::operation_aborted,
                            ec.message());
                    // Subsequent calls to close are aborted
                    ws.async_close({},
                        [&](error_code ec)
                        {
                            ++count;
                            BEAST_EXPECTS(ec == net::
                                error::operation_aborted,
                                    ec.message());
                        });
                });
            static std::size_t constexpr limit = 100;
            std::size_t n;
            for(n = 0; n < limit; ++n)
            {
                if(count >= 3)
                    break;
                ioc.run_one();
            }
            BEAST_EXPECT(n < limit);
            ioc.run();
        }
    }

    void
    testMoveOnly()
    {
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_ping({}, move_only_handler{});
    }

    struct copyable_handler
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    void
    run() override
    {
        testPing();
        testSuspend();
        testMoveOnly();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,ping);

} // websocket
} // beast
} // boost
