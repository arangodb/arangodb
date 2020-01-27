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

#include "test.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/write.hpp>

namespace boost {
namespace beast {
namespace websocket {

class read3_test : public websocket_test_suite
{
public:
    void
    testSuspend()
    {
        // suspend on read block
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
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            while(! ws.impl_->rd_block.is_locked())
                ioc.run_one();
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on release read block
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BOOST_ASSERT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on write pong
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // insert a ping
            ws.next_layer().append(string_view(
                "\x89\x00", 2));
            std::size_t count = 0;
            std::string const s = "Hello, world";
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(buffers_to_string(b.data()) == s);
                    ++count;
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    ++count;
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // Ignore ping when closing
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            // insert fragmented message with
            // a ping in between the frames.
            ws.next_layer().append(string_view(
                "\x01\x01*"
                "\x89\x00"
                "\x80\x01*", 8));
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(buffers_to_string(b.data()) == "**");
                    BEAST_EXPECT(++count == 1);
                    b.consume(b.size());
                    ws.async_read(b,
                        [&](error_code ec, std::size_t)
                        {
                            if(ec != net::error::operation_aborted)
                                BOOST_THROW_EXCEPTION(
                                    system_error{ec});
                            BEAST_EXPECT(++count == 3);
                        });
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // See if we are already closing
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            // insert fragmented message with
            // a close in between the frames.
            ws.next_layer().append(string_view(
                "\x01\x01*"
                "\x88\x00"
                "\x80\x01*", 8));
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != net::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.impl_->rd_block.is_locked());
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
            ioc.run();
            BEAST_EXPECT(count == 2);
        });
    }

    void
    testParseFrame()
    {
        auto const bad =
            [&](string_view s)
            {
                echo_server es{log};
                net::io_context ioc;
                stream<test::stream> ws{ioc};
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/");
                ws.next_layer().append(s);
                error_code ec;
                multi_buffer b;
                ws.read(b, ec);
                BEAST_EXPECT(ec);
            };

        // chopped frame header
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x81\x7e\x01");
            std::size_t count = 0;
            std::string const s(257, '*');
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(buffers_to_string(b.data()) == s);
                });
            ioc.run_one();
            es.stream().write_some(
                net::buffer("\x01" + s));
            ioc.run();
            BEAST_EXPECT(count == 1);
        }

        // new data frame when continuation expected
        bad("\x01\x01*" "\x81\x01*");

        // reserved bits not cleared
        bad("\xb1\x01*");
        bad("\xc1\x01*");
        bad("\xd1\x01*");

        // continuation without an active message
        bad("\x80\x01*");

        // reserved bits not cleared (cont)
        bad("\x01\x01*" "\xb0\x01*");
        bad("\x01\x01*" "\xc0\x01*");
        bad("\x01\x01*" "\xd0\x01*");

        // reserved opcode
        bad("\x83\x01*");

        // fragmented control message
        bad("\x09\x01*");

        // invalid length for control message
        bad("\x89\x7e\x01\x01");

        // reserved bits not cleared (control)
        bad("\xb9\x01*");
        bad("\xc9\x01*");
        bad("\xd9\x01*");

        // unmasked frame from client
        {
            echo_server es{log, kind::async_client};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            ws.next_layer().append(
                "\x81\x01*");
            error_code ec;
            multi_buffer b;
            ws.read(b, ec);
            BEAST_EXPECT(ec);
        }

        // masked frame from server
        bad("\x81\x80\xff\xff\xff\xff");

        // chopped control frame payload
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x89\x02*");
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(buffers_to_string(b.data()) == "**");
                });
            ioc.run_one();
            es.stream().write_some(
                net::buffer(
                    "*" "\x81\x02**"));
            ioc.run();
            BEAST_EXPECT(count == 1);
        }

        // length not canonical
        bad(string_view("\x81\x7e\x00\x7d", 4));
        bad(string_view("\x81\x7f\x00\x00\x00\x00\x00\x00\xff\xff", 10));
    }

    void
    testIssue802()
    {
        for(std::size_t i = 0; i < 100; ++i)
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // too-big message frame indicates payload of 2^64-1
            net::write(ws.next_layer(), sbuf(
                "\x81\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff"));
            multi_buffer b;
            error_code ec;
            ws.read(b, ec);
            BEAST_EXPECT(ec == error::closed);
            BEAST_EXPECT(ws.reason().code == 1009);
        }
    }

    void
    testIssue807()
    {
        echo_server es{log};
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        ws.write(sbuf("Hello, world!"));
        char buf[4];
        net::mutable_buffer b{buf, 0};
        auto const n = ws.read_some(b);
        BEAST_EXPECT(n == 0);
    }

    /*
        When the internal read buffer contains a control frame and
        stream::async_read_some is called, it is possible for the control
        callback to be invoked on the caller's stack instead of through
        the executor associated with the final completion handler.
    */
    void
    testIssue954()
    {
        echo_server es{log};
        multi_buffer b;
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        // message followed by ping
        ws.next_layer().append({
            "\x81\x00"
            "\x89\x00",
            4});
        bool called_cb = false;
        bool called_handler = false;
        ws.control_callback(
            [&called_cb](frame_type, string_view)
            {
                called_cb = true;
            });
        ws.async_read(b,
            [&](error_code, std::size_t)
            {
                called_handler = true;
            });
        BEAST_EXPECT(! called_cb);
        BEAST_EXPECT(! called_handler);
        ioc.run();
        BEAST_EXPECT(! called_cb);
        BEAST_EXPECT(called_handler);
        ws.async_read(b,
            [&](error_code, std::size_t)
            {
            });
        BEAST_EXPECT(! called_cb);
    }

    /*  Bishop Fox Hybrid Assessment issue 1

        Happens with permessage-deflate enabled and a
        compressed frame with the FIN bit set ends with an
        invalid prefix.
    */
    void
    testIssueBF1()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

        // read
#if 0
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            net::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            net::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // invalid 1-byte deflate block in frame
            net::write(wsc.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
            error_code ec;
            multi_buffer b;
            wss.read(b, ec);
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }

        // async read
#if 0
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            net::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            net::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // invalid 1-byte deflate block in frame
            net::write(wsc.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
            error_code ec;
            flat_buffer b;
            wss.async_read(b,
                [&ec](error_code ec_, std::size_t){ ec = ec_; });
            ioc.run();
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }
    }

    /*  Bishop Fox Hybrid Assessment issue 2

        Happens with permessage-deflate enabled,
        and a deflate block with the BFINAL bit set
        is encountered in a compressed payload.
    */
    void
    testIssueBF2()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

        // read
        {
            net::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // contains a deflate block with BFINAL set
            net::write(wsc.next_layer(), sbuf(
                "\xc1\xf8\xd1\xe4\xcc\x3e\xda\xe4\xcc\x3e"
                "\x2b\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\x1e"
                "\x36\x3e\x35\xae\x4f\x54\x18\xae\x4f\x7b"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\xe4"
                "\x28\x74\x52\x8e\x05\x74\x52\xa1\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\x36\x3e"
                "\xd1\xec\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e"));
            error_code ec;
            flat_buffer b;
            wss.read(b, ec);
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }

        // async read
        {
            net::io_context ioc;
            stream<test::stream> wsc{ioc};
            stream<test::stream> wss{ioc};
            wsc.set_option(pmd);
            wss.set_option(pmd);
            wsc.next_layer().connect(wss.next_layer());
            wsc.async_handshake(
                "localhost", "/", [](error_code){});
            wss.async_accept([](error_code){});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(wsc.is_open());
            BEAST_EXPECT(wss.is_open());
            // contains a deflate block with BFINAL set
            net::write(wsc.next_layer(), sbuf(
                "\xc1\xf8\xd1\xe4\xcc\x3e\xda\xe4\xcc\x3e"
                "\x2b\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\x1e"
                "\x36\x3e\x35\xae\x4f\x54\x18\xae\x4f\x7b"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\x1e\x36\xc4\x2b\x1e\x36\xc4\x2b\xe4"
                "\x28\x74\x52\x8e\x05\x74\x52\xa1\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e"
                "\xd1\xe4\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4\x36\x3e"
                "\xd1\xec\xcc\x3e\xd1\xe4\xcc\x3e\xd1\xe4"
                "\xcc\x3e\xd1\xe4\xcc\x3e"));
            error_code ec;
            flat_buffer b;
            wss.async_read(b,
                [&ec](error_code ec_, std::size_t){ ec = ec_; });
            ioc.run();
            BEAST_EXPECTS(ec == zlib::error::end_of_stream, ec.message());
        }
    }

    void
    testMoveOnly()
    {
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_read_some(
            net::mutable_buffer{},
            move_only_handler{});
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
    testAsioHandlerInvoke()
    {
        // make sure things compile, also can set a
        // breakpoint in asio_handler_invoke to make sure
        // it is instantiated.
        {
            net::io_context ioc;
            net::strand<
                net::io_context::executor_type> s(
                    ioc.get_executor());
            stream<test::stream> ws{ioc};
            flat_buffer b;
            ws.async_read(b, net::bind_executor(
            s, copyable_handler{}));
        }
    }

    void
    run() override
    {
        testParseFrame();
        testIssue802();
        testIssue807();
        testIssue954();
        testIssueBF1();
        testIssueBF2();
        testMoveOnly();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read3);

} // websocket
} // beast
} // boost
