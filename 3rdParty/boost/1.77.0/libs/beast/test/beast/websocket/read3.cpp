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

    /*
     * Tests when a deflate block spans multiple multi_byte character arrays.
     */
    void
    testIssue1630()
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;

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

        const asio::const_buffer packets[] = {
            sbuf(
                // websocket bytes
                "\xc1\x2d"

                //deflated payload
                "\xaa\x56\xca\x4b\x4d\xcf\x2f\xc9\x4c\x2c\x49"
                "\x4d\x09\xc9\xcc\x4d\xcd\x2f\x2d\x51\xb2\x32"
                "\x35\x30\x30\xd0\x51\x2a\xa9\x2c\x48\x55\xb2"
                "\x52\x2a\x4f\xcd\x49\xce\xcf\x4d\x55\xaa\x05"
                "\x00"
            ),
            // packet 2
            sbuf(
                "\xc1\x7e\x0b\x89"
                "\xd4\x5a\xdb\x8e\xdb\x48\x92\xfd\x15\xa1\x1e"
                "\x17\x5b\xae\xbc\x5f\xb8\xf0\x43\x49\xa5\xc2"
                "\xf6\xa2\xdd\x30\xfa\x36\x98\xd9\x19\x34\xf2"
                "\x12\x29\xb1\x74\x2d\x92\x52\x5d\x0c\xff\xfb"
                "\x1c\x4a\xb6\xdb\xa2\xc7\xd3\xe8\x92\x5f\x06"
                "\x7e\x28\x93\x99\xc9\xc8\x8c\x38\x11\xe7\x04"
                "\xa9\x77\x17\x71\x93\x9f\x2e\xaa\x77\x17\x7b"
                "\x6a\xda\x7a\xb3\xbe\xa8\xc4\xa7\x59\x69\xb3"
                "\x5e\x53\xc2\x43\x2e\xfe\xfb\x62\x4b\xd4\xfc"
                "\xf6\xe1\x06\xa6\xb5\x17\xd5\xff\xbf\xbb\xa8"
                "\x33\x66\x69\x9a\x46\xca\x21\xa4\xac\xcd\xf8"
                "\xd6\x14\xaf\xb8\x4f\x79\x62\xb3\x1c\x3b\x3d"
                "\xe1\x1e\x8b\x33\xb5\xa9\xa9\xb7\xdd\xe1\xf9"
                "\xef\x3e\x3e\x3e\xac\xdb\x07\x6a\x30\xdc\xe6"
                "\x2d\x2e\xf7\xaf\xd9\xdf\x9b\xbf\xaf\x37\xaf"
                "\x2f\x47\xd2\x09\xa5\xad\x36\x52\x7d\xfe\xdf"
                "\xef\x7e\x18\x7d\xf7\x56\x8d\xd8\xab\xc3\xbf"
                "\x7e\x6e\xfb\xfa\xd7\x37\xc4\x44\x14\x26\x58"
                "\x91\xb2\x30\x2c\x6a\x92\x32\x58\xed\x42\x0c"
                "\x2c\x59\x8a\xb1\x9f\xd7\xbd\x66\xa3\xc3\x82"
                "\xf0\xba\x4e\x74\xb9\xac\x3b\x3a\x5e\xad\xda"
                "\x3a\x5f\xb6\xb4\x0a\xeb\xae\x4e\xd5\xe8\x2f"
                "\x6f\x7e\x1a\xfd\xd7\x71\x64\xd6\x6c\x76\xdb"
                "\x6a\xfc\xcb\x0f\x37\xdf\x4f\x47\x61\x97\xeb"
                "\xcd\x68\x5f\x67\xda\xf4\xa3\xab\xd7\xc7\x1b"
                "\xdc\x70\x2e\x46\xbf\xdc\xbc\xbd\xfa\xf9\xfb"
                "\x9f\xae\x7e\xfc\xf9\xed\xd5\x4f\xd7\xbf\xbe"
                "\xbd\x1d\x71\xce\x8f\xe6\xd2\xeb\x0f\x7b\xd6"
                "\xea\x15\x37\xf2\x95\x32\xaf\x38\x37\x47\x03"
                "\xf4\xd8\xad\xc2\xb6\xe2\xa3\x5d\xb3\xae\x6a"
                "\xea\x4a\xb5\x0d\x4d\x58\xb5\x55\xd3\x6d\x2f"
                "\xe7\xb9\xc1\x78\xd5\xb6\x4d\xba\x3c\xd8\xba"
                "\x5c\xd2\x9e\x96\xc7\x95\x4d\x97\xb0\xee\x60"
                "\xfb\xeb\x8f\xc7\x53\x0e\x8f\xc7\x56\x36\xdb"
                "\x5d\x7b\xa5\x1c\xe2\x7f\x25\x4e\x06\xd9\xe8"
                "\xed\xe4\xcd\x2f\x57\xfd\xc8\xf1\x7e\x59\x75"
                "\xc7\x25\xab\x7a\x8d\x68\xad\xe8\x35\x67\xff"
                "\xb3\x6b\xa9\x5e\xc7\xb0\xce\x85\xd2\x6b\x7e"
                "\x9c\x98\x70\x59\x67\xe0\xab\x12\x23\xde\x7b"
                "\x60\x24\x18\x97\xc2\x18\x25\xd9\xe9\x6e\x3e"
                "\x38\x09\x21\x1f\xcd\x37\x6d\xf7\xe5\x72\x71"
                "\xb2\x5c\xf8\x2f\x96\x3b\xad\x06\xcb\x7b\x07"
                "\x5c\xae\x76\x8f\xc7\xab\x96\x3a\x44\x2a\x00"
                "\x96\xfb\x8f\x51\xad\x73\x75\xf0\xda\x87\x90"
                "\xaf\x3f\x1f\xec\x01\xb0\x2b\x4d\x98\x55\x7f"
                "\x91\xe3\xd5\xef\xf7\xb6\x0f\xb9\xfa\xbf\xc9"
                "\xf7\xe5\x6f\xbf\x88\xe4\x36\xfc\xfb\xbf\xbe"
                "\xf9\xa1\xb9\x7e\x93\xfe\xf7\xe9\xbb\x0f\xbe"
                "\xa9\xd7\x33\x6a\xb6\x4d\xbd\x46\x5c\xe6\xe1"
                "\x52\x68\x33\xba\xbe\xa9\xac\xa8\xa6\x93\x4a"
                "\x89\x8a\x4d\x2a\xc9\xaa\xa9\xad\x1c\xaf\x8c"
                "\xad\xae\x27\x15\x9f\x56\xb7\xaa\x62\xbc\x9f"
                "\xa3\x58\x35\xbe\xae\xa4\xab\xb8\xae\xac\xab"
                "\xec\x4d\x25\x54\x75\x73\x53\xe9\x49\x7f\x67"
                "\xcc\xab\x9b\xdb\xca\x4c\x2b\x31\xae\x24\xee"
                "\x4f\xfb\xfb\x52\x1f\xb1\x76\x80\xdd\x57\xb1"
                "\xe6\xe1\x22\x86\x20\x08\xfb\x47\x80\x3b\x78"
                "\x0d\x39\xfe\x4c\x27\x00\x14\xa3\x79\x87\xa8"
                "\x5f\x5d\x3d\x3c\x3c\xbc\x7a\xa0\x88\x69\xaf"
                "\x36\xcd\xec\x8a\x1e\xb7\xd4\x00\x02\xeb\xae"
                "\xbd\xfa\x1d\x92\x57\x21\xb6\x48\x98\x75\xbe"
                "\xec\xe1\xf1\xe7\xf1\x88\xed\xfe\xfa\xd6\x5d"
                "\x79\xf6\x09\x73\x1f\x81\x8a\x43\x34\x94\xff"
                "\xd5\x88\xb0\xa3\xdd\x72\x0b\xf4\x9d\x0e\xe2"
                "\x34\x25\xf6\x0f\x9c\x6d\x36\xb3\xcb\x86\x56"
                "\xf1\x8b\x91\x94\x56\xa3\x52\x37\x5f\xdc\x07"
                "\x20\x16\xff\xf2\xe6\x68\xbb\xac\xff\x53\x10"
                "\xfe\xa9\x1c\xfd\x87\x23\x1c\x04\xd0\xd0\xbe"
                "\x3e\x92\x0f\x7f\xff\xfe\x1f\x20\x84\x03\xb1"
                "\xfc\xf8\x26\x4a\xad\xb2\x92\x96\x9c\x0d\x14"
                "\x6d\xe1\x41\x48\x23\xbc\x29\x4a\xd8\xa0\x55"
                "\xc2\xd2\x75\x58\xf5\x5c\xd2\x51\xdb\xfd\xb8"
                "\xd9\xac\x78\x4f\x55\xa1\x41\x35\xaf\xb7\x28"
                "\xea\x3d\xdb\x1c\x1f\xf6\xf6\xda\x07\x6e\x98"
                "\xd6\x4c\xa9\x98\x5d\xe4\x31\x07\xe5\xb8\x35"
                "\xcc\x5a\xb2\x9a\x97\x88\x95\xf0\x28\x88\xa0"
                "\x03\x1f\x5e\xa8\xc5\x83\x6c\x1f\x1e\x14\x17"
                "\x01\x03\x5d\x03\x70\xf4\x9c\xf7\x8f\x93\xcd"
                "\x62\xa7\x1d\x62\x7c\x42\x95\xef\x4f\x36\x70"
                "\xe4\xc9\x8f\x5b\x50\xc9\x0a\xc5\x48\x27\x6e"
                "\x9d\xd1\xc1\x11\x27\x2a\xdc\x32\xe2\x9a\x7b"
                "\x7b\xb0\xf4\xd9\x16\x9e\x42\xfd\x1c\xb5\xdf"
                "\xdc\xcf\xfe\xec\x0e\x7e\xb7\xe8\x6c\xe4\x89"
                "\x31\xee\x75\x24\x09\x6b\x8a\x60\x8c\x65\xa5"
                "\x8b\x2f\x52\x65\x7e\x6a\x71\xbe\xb2\xdd\x7e"
                "\x3d\x8b\x0f\xdb\x97\x5b\x14\x64\x43\x20\xc9"
                "\x02\xf7\x4a\x38\xa1\x09\x8c\x2c\xa4\x73\x41"
                "\x30\x11\x92\x33\xa7\x16\x1f\x9f\x77\x6c\xbe"
                "\xb7\xba\xac\x4f\x4c\xbe\xbb\x58\xd4\xeb\xfe"
                "\x79\x87\x32\xde\xc7\xb4\xa9\x37\xcd\x71\x09"
                "\xcc\xaf\x73\x68\xf2\xe1\x41\xb8\x9e\xc4\x20"
                "\xd4\x6d\x9a\x4e\xb9\x1f\x73\x91\x6f\xf9\xd4"
                "\xfa\xb1\xb3\x53\x75\xe3\xfc\x58\xc8\x6b\xcc"
                "\xa3\x75\x88\x4b\xec\xb3\xea\x9a\x1d\x7d\x84"
                "\xd7\x9b\x9f\x59\x26\x15\x95\x4b\x91\x9b\x42"
                "\xe4\x88\x22\xb6\x18\x12\xe5\x92\x99\x96\xe5"
                "\x33\x78\x1d\xb6\xd1\x3b\xeb\xe3\xd9\x1b\x0a"
                "\x90\x4d\xef\xff\xa4\x6b\xa4\xcd\x3a\xc9\x10"
                "\xe0\x0d\x66\x45\xb1\xc5\x15\x0a\x0e\x7f\x1c"
                "\x15\x16\xf4\xa9\x6b\xb6\xcb\xfc\x3c\x7f\x7e"
                "\x6e\x82\x7b\x79\x30\x4a\x61\x52\x10\x03\xd4"
                "\x11\x13\x32\x8a\x2b\x0b\x81\xa6\x2d\x67\x64"
                "\x6c\xa4\x01\xe0\x9c\x7a\x5e\x6d\xd6\x4f\x33"
                "\x7b\x86\x45\x29\xb3\x13\xdc\x05\x0f\x9f\x96"
                "\x60\x88\xbc\x0c\x39\x51\x04\xe8\x45\x31\x7c"
                "\x60\x71\xbf\x17\xcb\xd9\x4c\x3f\xab\xfb\xf8"
                "\x72\x93\x54\x10\x45\x98\xd3\x46\x91\xe3\xc5"
                "\x67\xe3\x78\x72\xd1\xb3\xe2\xb9\x04\xf2\x4e"
                "\x4d\x2e\x6a\x73\x37\x33\xe6\x69\x7f\x77\x7f"
                "\x86\x49\x99\x62\xcc\x49\x3a\x29\x54\x91\x92"
                "\x21\x9f\x19\x59\x11\x93\xb5\x70\x36\xf9\x53"
                "\x93\xcf\x49\x3c\x74\xeb\x55\x9a\xe9\x97\x62"
                "\xdc\x8e\xbd\x4c\xc0\x8b\x08\xd7\x65\x12\x88"
                "\x29\x76\x33\xf6\x88\xa7\xb9\xe5\x66\x32\x66"
                "\xe6\xeb\x18\x2f\x36\x1a\x53\x5c\x2e\xdc\xbb"
                "\x88\x24\x8c\x56\x81\x83\x94\x00\xf4\x82\xc8"
                "\xd1\x7d\x6b\x8c\x67\x16\xb1\x53\x6f\x6d\xd4"
                "\x2c\x3b\x16\x89\x39\xe1\xc9\x9b\xbe\x80\x1b"
                "\x00\xe2\xd4\x33\x65\x66\xee\xb6\xcf\x0f\x4e"
                "\x29\xf3\x52\xd7\xe4\x6b\xe7\x39\xbf\xa1\xa8"
                "\x8c\x33\xe4\x99\xb8\x71\xc5\x0a\xeb\x43\x36"
                "\x13\x19\xb4\xfb\xba\x6b\x28\x18\x99\x8d\x28"
                "\x49\xea\x4c\x5a\x0b\xa9\x4a\x22\x1e\x5d\xc9"
                "\xdc\x07\x55\xbe\xb9\x6b\xa2\x96\x46\x72\xa6"
                "\x85\x8e\x46\x64\x50\x18\x2a\x30\x17\xe8\x57"
                "\x94\xc9\x3c\x85\x01\x01\x2d\x5a\x24\xd1\xd3"
                "\xc2\xad\xd9\xfc\x0c\x9c\x22\x1a\x21\xe4\x10"
                "\xbd\x96\xde\x71\x27\x8b\x62\x20\x40\x27\x72"
                "\xee\xb7\x30\x30\xa9\xbb\xd6\x7b\xbf\x8f\xfb"
                "\xae\x9c\x51\xff\x59\x30\x3e\x5a\x9f\x35\x22"
                "\xae\x72\x62\x9e\x64\xe6\x1c\x65\xd5\x19\xc1"
                "\xe3\xa0\xc8\xb5\x6e\xb3\x69\x45\x3b\x67\x67"
                "\x70\x5c\x4a\xbd\x28\xc8\x92\x2b\x9e\x2c\x7a"
                "\x41\x69\xa8\x38\xc4\x4f\x4a\x6e\x75\x1e\x42"
                "\x4e\x37\xdb\xc7\xed\xfe\xa9\x7e\xf4\x67\x98"
                "\x54\x85\x83\x20\x14\x05\x1c\x0e\x96\xad\x0b"
                "\x8a\xf3\x62\xb2\xd3\x01\x95\x5d\x0e\x68\x35"
                "\xd7\xb6\x5d\x28\xf5\x94\xd5\x4b\x41\x7e\x33"
                "\x9e\x06\xba\x4d\xd7\xb6\x8c\x7b\x01\x33\x65"
                "\x13\x7d\x3b\x09\xce\x84\x58\x8c\xbe\xfd\x77"
                "\xf9\x0f\x81\xa3\x9d\xb4\x5c\x1a\x28\x0c\xaf"
                "\x28\x16\x8d\x7a\xec\x3c\x01\x7f\x42\x90\xfa"
                "\xd6\x20\x2f\x59\xd9\xac\x10\x74\x4b\xc9\x2b"
                "\x23\xb4\x48\xd2\xfb\xc4\xa3\xe1\xca\xcb\xa1"
                "\xc4\x29\x6c\x7f\xcf\x3b\x7b\x87\x04\x7c\x79"
                "\x30\xb4\x06\xcb\xe5\x98\xb3\x43\xa5\x8b\x05"
                "\xa7\xcb\x7d\xa3\x20\x20\x06\xa0\xa9\x6c\x3e"
                "\x35\x69\x15\x28\xc1\x77\x35\x70\xf7\x72\x93"
                "\x28\x10\xc6\x6b\x21\x58\xce\x64\x7d\xca\x3c"
                "\x6b\x6b\x74\xb4\x89\xf3\x64\x24\x1b\x80\x9c"
                "\xb4\xcf\x77\x4f\x51\x24\x75\x06\xe5\x20\xa1"
                "\x24\x8b\xde\xb8\xa0\x21\x7f\x0b\xca\x88\xa3"
                "\x24\xc8\x69\xb2\xc9\x17\xaf\x06\x54\xce\xc0"
                "\x87\x8b\x8d\xe8\x16\xf9\x1c\x94\x33\xa3\xbd"
                "\x88\x92\x41\xb7\xa5\x22\x7d\x48\x06\x7a\x5c"
                "\xf3\x04\xe6\x30\x43\xc5\xcc\x96\x9d\xae\xe7"
                "\xa1\xd6\x85\x2f\xcf\x08\x26\x2a\x07\xa0\xe9"
                "\xa3\x57\xc5\x3a\x5c\xf9\x08\xa7\x92\x56\x88"
                "\xa7\x31\x76\xc0\xac\x82\x4c\x57\x5a\xb5\x62"
                "\xcf\x2f\xcd\xac\x08\xed\x2d\x85\xe7\xf6\x7a"
                "\x72\xdd\x87\x6e\x22\xd9\x58\x4b\x3b\x9e\x5e"
                "\x6b\xe5\xc6\x53\xff\xf5\xcc\xf2\x94\x1c\x73"
                "\xc8\x78\xe6\x25\xa9\xa0\x50\xce\x0b\x17\x89"
                "\x07\x12\x51\xf0\x03\xec\xbe\xad\x7a\xcc\x26"
                "\x07\xcd\x0c\xe3\x09\x6c\x0e\xba\x42\x95\x8d"
                "\x29\x12\x1a\x24\xc1\x58\x11\x03\xf5\x58\x3f"
                "\x85\xc7\x4d\xb1\x07\x8f\xbd\x58\xe5\x14\x70"
                "\xab\xe3\x26\xf6\x2f\x0f\xa3\x89\x46\x8b\xec"
                "\x3c\xf4\x0d\x2a\x2e\x2b\x03\xc8\x41\x68\x3e"
                "\xee\xfd\x2a\xcf\xdb\xc5\xcb\x4d\x32\x74\xc4"
                "\xc9\x68\xcb\xd0\x2a\x33\x1e\x20\x8a\x93\xb1"
                "\xc5\xea\xc8\x8c\x09\x65\x78\xc8\xa7\xe2\xb6"
                "\xe9\x7e\xbe\xdb\xbd\xb8\xb0\x2a\x88\xa7\x5b"
                "\x75\x7d\x33\xd5\x37\x19\x9a\x65\x2c\x99\x74"
                "\xe3\x34\x1d\x5f\xc7\x89\x98\x18\x16\xbe\x1e"
                "\x7e\x21\x2c\xb7\x05\x8d\x23\x68\x2e\xa0\xfe"
                "\x18\x0a\x0c\xd1\x4f\x5e\xa3\xf0\x45\xff\xcd"
                "\xd5\x03\x11\x4b\xa0\x18\x86\x36\xd6\x2a\xab"
                "\x60\x11\xea\x26\x65\x05\x3f\x81\xce\xf5\xc0"
                "\x33\xcd\x12\xcd\xe5\xc2\xb9\xfb\xa7\xfa\x0c"
                "\x62\x25\xa9\x24\x1a\x13\x69\x8d\xcb\x21\x43"
                "\x60\x43\xc4\x43\xcb\x3b\xef\x91\x99\x21\x0f"
                "\x4d\xf2\x9d\x70\x7b\x12\x67\x55\xb9\x82\x63"
                "\xf1\x04\x77\xa2\x39\xf5\x26\x66\x54\x20\x97"
                "\x24\x8f\x92\x52\x89\xce\x9e\x9a\x5c\xd7\x4a"
                "\x2f\x96\x4d\x58\x75\x2f\x8d\xbf\x80\xc1\x88"
                "\xfe\x54\x66\x81\x7e\xe8\x5a\x8c\xf3\x4d\x0c"
                "\x13\x48\xf9\x9b\x71\x30\xec\x76\xf2\x6f\x9a"
                "\x47\x42\xb0\x45\xd1\xce\x41\x8d\x17\xe1\x0c"
                "\xaa\x15\x49\x13\x15\xd3\x11\xac\xf7\xcd\xe3"
                "\xcf\xd0\xbd\x5b\x6d\xa1\xdf\x93\x46\x00\x7c"
                "\x28\x9c\x02\xaa\x80\x40\x4b\xaf\x50\xec\x07"
                "\x9e\x69\xd5\xbc\xbd\xdb\xda\x99\x3f\xa7\xfe"
                "\xa3\x6b\x04\xc8\x23\xe3\x4c\x5a\x2b\x50\x6f"
                "\xb2\x08\x05\x98\x4b\xac\x48\x32\x03\x61\x25"
                "\xd9\x66\xe1\x54\x36\xfb\xbb\x33\x84\x15\x13"
                "\x25\x2a\xc1\x55\xce\x90\x55\x46\x95\x08\x75"
                "\xac\x4c\x42\x5b\x17\xb9\x14\x76\x60\x72\x89"
                "\xe6\x71\x1e\x9a\xe6\xa9\x9c\xa1\x91\x21\x8d"
                "\x20\x87\x99\xd5\xc6\x27\x05\x69\xee\x93\x83"
                "\x2c\xe7\xb0\xcb\x89\xcb\x61\xc9\x11\x8f\xbb"
                "\x0d\x7f\x94\xf3\x66\x25\x5f\x6e\xd2\x16\x43"
                "\x82\xc9\xe8\x98\x8f\x32\x17\x54\xb8\x52\x28"
                "\x5b\xe5\x0b\xca\x1e\x94\xd3\xe0\x1d\x49\x9c"
                "\x2f\x56\x1b\x41\xeb\xd9\x19\xb1\x24\x88\x90"
                "\x92\xb8\x62\x89\x60\xd4\x48\x95\x99\x77\x49"
                "\x40\x27\x08\x19\xfd\x50\x24\xdf\xab\xf5\xec"
                "\x7e\x1d\x9e\x66\x74\x46\x5f\x2e\x1d\x8a\x93"
                "\x83\x5c\x40\xf4\x54\x41\x17\x58\x38\xb2\x59"
                "\x05\x10\xab\x4b\x5c\x0c\xa8\xfc\xfe\xa1\x9d"
                "\xdf\xdd\x6f\x1f\xdb\xc5\x19\x26\xb5\x46\x4a"
                "\x90\x02\x91\x27\x4a\x56\xb3\x90\x82\x70\x96"
                "\x34\x7a\x0f\x2a\x5a\x0c\x14\x4b\x11\x4d\x6a"
                "\xbc\x9c\x69\x7d\x86\x60\xa1\xc8\xc1\xfa\xa6"
                "\x7f\x9b\x16\x13\x9a\x0e\x83\x5e\x24\x59\x2f"
                "\xac\x8a\x68\x77\xfd\xa0\x62\xdd\x89\x6d\xbd"
                "\x60\x1d\xd9\xbb\x33\xd2\x32\x40\x05\x9a\xfe"
                "\x2d\xa8\x43\xd7\xe8\x11\x41\xed\x83\xd0\xc1"
                "\x16\x01\x0c\xbb\x32\xf0\xab\xdc\xb7\x16\x5a"
                "\xe0\x41\x9d\xa1\x77\x21\x3d\x95\x44\x65\x87"
                "\x9e\xb0\x31\xa0\xde\x59\x1b\x12\x72\x25\xa1"
                "\x5a\x7b\x2e\x07\xc5\xa7\xf8\xc5\x96\xa7\xd6"
                "\xdd\xdb\xcd\x19\x4c\x60\x81\x14\x99\x90\xfd"
                "\xa5\x7f\x7f\x84\x4e\x27\x6b\x86\x8c\x81\x34"
                "\xb0\x28\xda\x83\xb4\x94\x4f\xbe\x70\x7e\x9f"
                "\xf7\xea\x8c\x1c\xd1\xc1\xa3\x9d\x82\xce\x4d"
                "\xe0\x58\x1c\x58\x3a\xce\x73\x16\xae\x6f\x97"
                "\x55\x09\x83\x50\xba\xa5\x5c\x74\x7c\x2f\x50"
                "\x12\xcf\xa0\x58\x0f\x42\x47\x6e\xa2\x8e\x73"
                "\xc6\x7d\x12\x3c\x28\x17\x4c\x80\xf4\x81\x86"
                "\x18\x9e\x92\xb9\x36\x85\xc5\x92\x67\x1b\xce"
                "\xf0\x6c\xdf\x2f\xf9\xe0\x55\xcc\x85\x65\xe5"
                "\x72\x71\x56\x94\xe2\xb3\x4d\x21\x1b\x68\xfc"
                "\x53\x9b\x3c\x3e\xec\xed\xbe\xcc\xeb\xd4\x9d"
                "\x61\x32\x6a\x30\xad\xb7\xc5\x17\x65\x05\x79"
                "\x6f\x38\x0a\x81\x4a\xde\xeb\xe8\x85\x92\x83"
                "\xf7\x65\xeb\xf6\xfe\x49\x75\xbb\xe7\x17\xcb"
                "\x3a\x06\x76\x1e\x4f\xc1\x20\x29\x38\x39\x9e"
                "\x4c\x7a\x3a\x17\x60\x4f\xe2\xd7\x5a\xdc\x9a"
                "\xf4\x75\x5a\xef\x9b\x0f\x85\x02\x49\xde\x80"
                "\x12\xc8\xf9\x48\x2c\x6a\x8f\x52\x96\xb2\x06"
                "\x00\xbf\x79\xbf\x6c\xd1\x43\x42\xab\x25\xd5"
                "\x7f\x9d\x50\x46\x73\x21\x0d\x49\x9d\x6c\xf4"
                "\x68\xf2\x06\xfd\xf2\x03\xbc\xb6\x5c\x47\x3b"
                "\x13\x67\x50\x2c\x54\x2b\xa1\x19\x85\xb6\xd2"
                "\x09\x0a\x3f\x87\x5e\x63\x99\xe0\xac\xe3\xe4"
                "\x86\xed\xf2\x5e\xb3\x25\x37\x8b\x8d\x6f\xcf"
                "\x78\x29\x8c\xd3\xf5\xe2\x05\x9a\x35\x14\xc8"
                "\x7b\x54\x62\xc8\xe6\xec\x82\x30\xa4\x8d\xf4"
                "\x83\xd7\x25\x89\x9a\xb6\xd3\x72\xc5\xba\x33"
                "\x8a\xa4\x64\x60\x73\x74\x11\x94\xd0\xb8\x40"
                "\x51\x58\x69\x23\x57\x04\xda\x29\xe0\x77\x36"
                "\x40\xdc\x02\x4d\xdc\xbc\x7f\x63\xf4\x67\x31"
                "\x7e\x98\x92\x36\x4d\xae\xd7\xb3\xfe\x63\xd3"
                "\x27\x54\x95\xb0\x6c\x69\xb0\xbe\x6e\x7f\xfb"
                "\x6c\xf2\x61\xc6\x7b\x3c\x75\x17\xfb\x5f\x47"
                "\xc4\x7e\xd5\xbb\xcf\x16\xb0\xcf\x77\xd2\x7f"
                "\x55\xda\xc5\x65\xdd\xce\x87\xd3\xf8\x60\x5a"
                "\x4b\xed\x71\xe0\x42\xab\x83\xae\x61\x21\x78"
                "\x70\xa2\x76\xc5\x23\x0e\x68\x1b\x7d\x08\xa0"
                "\x61\x50\xc7\x1f\x7c\x5d\xfb\xa3\x0f\x66\x16"
                "\x09\x0c\xa5\xa4\xa2\xce\x8c\x65\x23\x79\x36"
                "\xa5\x58\x10\x93\x64\x42\xb9\x03\x29\x6c\xb6"
                "\x1f\x7e\x2c\xf2\xee\x62\x45\xb9\x0e\x38\xff"
                "\xec\xb8\xb9\x5d\x7b\x48\x9d\x7a\xb6\x0e\x4b"
                "\xb8\xe3\x8b\x81\xe3\x29\x7e\xeb\x3e\xfe\x7c"
                "\x45\xb2\xcf\x67\x53\x9e\x1d\x52\xb0\x9d\xc7"
                "\x5d\xb3\xbe\x78\xff\xfe\xd3\x6f\x56\x56\xed"
                "\xec\xe2\xfd\x3f\x01"
            ),

            // packet 3
            sbuf(
                "\xc1\x7e\x02\x73"
                "\xec\x9a\xc1\x6e\xdc\x20\x10\x86\x5f\x25\xf2"
                "\x7d\x24\x98\x1d\x83\xc9\xa9\xaf\x02\x36\x3e"
                "\x45\x4d\xd4\x34\x95\x7a\xc8\xbb\xf7\xf7\xaa"
                "\xf2\x42\xb5\xad\x41\xe9\x4a\xdd\x6a\x6e\x7b"
                "\xd8\xf9\xc7\x06\xcc\xfc\x03\xdf\x1f\xa9\x96"
                "\xb7\x97\xed\x5e\x76\xb8\x09\xd2\xf2\x0c\x1f"
                "\xd7\x4c\xb4\x8c\x1f\x20\x5a\xf6\x8b\xfe\x22"
                "\xf6\x56\x94\x4b\x13\xe1\x52\x3c\xc7\xad\xe1"
                "\x93\x33\x6f\x10\xae\x0c\xde\x47\xa0\x9a\x7f"
                "\xf2\xbe\xff\x25\xbe\xbe\x5e\x45\x5a\x36\x02"
                "\x03\x5b\xc9\xb7\xbb\xbc\xf0\xc7\xe3\x63\x0a"
                "\xf0\x27\x0c\x6d\x18\x31\x32\x0f\xf3\x56\x52"
                "\x1f\x31\xd7\x5f\xb0\x51\xc3\x4e\xa2\x14\x85"
                "\x4f\xdb\xe8\x10\x9c\x18\x3e\x06\x5e\xaf\xc7"
                "\x6d\x0b\xfb\x71\xc9\x6b\x7c\x7b\xfa\xfa\x10"
                "\x56\xb4\x53\x30\xfe\x24\x56\x98\xb0\x2b\x19"
                "\x0a\x1c\x3c\xa1\xa8\x6e\x95\x34\xe0\x9b\x2a"
                "\x65\x60\x9a\x27\x58\x2e\x6b\xa7\xbe\xf4\x45"
                "\x5c\x95\x7e\x46\x23\xc5\x61\x74\xe4\xc3\x12"
                "\x09\x2e\xc7\x11\xf6\xc0\x4c\x0b\x7a\x58\x9f"
                "\xd2\x82\xdf\x4b\x29\x33\xf9\x33\x04\xc0\xa6"
                "\x33\xfd\x25\xae\x4a\xcf\xe1\x34\x39\xc3\x33"
                "\xa1\x95\x74\x84\x36\xdc\x52\xb2\x21\x51\xf0"
                "\xd6\xc2\x46\xe7\x6c\xf3\x5c\xc8\x9c\xb0\x84"
                "\x9d\x4c\xa3\xe3\xbe\xf4\x45\x5c\x95\xde\xa5"
                "\x1c\xcd\x8a\x21\xb7\x12\x13\x89\x99\x84\x42"
                "\x9e\x22\x39\x98\x1b\x19\x67\x4e\x4b\xf0\x95"
                "\x8c\x20\x51\x40\x47\xdf\x9b\x7e\x8f\xab\xd2"
                "\x8f\x28\x50\xc6\x4c\x4c\xc9\xb1\x25\x81\x99"
                "\xa1\xed\x66\x84\x5c\xe4\x24\x78\x10\x4e\x59"
                "\x4a\x19\xd4\x35\x73\x82\xdd\xea\x9c\xfb\x22"
                "\xae\x7e\x7b\x37\xa1\x13\xf2\x91\x4c\x90\x13"
                "\xd6\x9f\x77\x14\xd3\x6c\xb0\x14\x04\xb6\x0e"
                "\x7b\x97\x49\xb5\x0c\xa3\x8f\x32\xe2\x7b\xdf"
                "\xfe\x12\x57\xa5\x4f\x92\x38\x25\x83\xa5\x0e"
                "\x1b\x47\x02\x37\x49\xe8\xeb\x85\x22\x2c\x50"
                "\xf2\x28\xf9\x6b\x0e\x85\x0c\xf6\x24\xf6\x58"
                "\xaa\x63\x5f\xf6\x4b\x58\x95\xdc\x66\xb1\x71"
                "\x11\x64\x83\x7d\x20\x58\x87\x89\xe2\xcc\x81"
                "\xe0\x5d\xe3\x92\x45\xd8\x2c\x5c\x81\x6c\xed"
                "\x10\xdb\xb5\x9a\xf2\x57\x01\xb2\xdf\x54\x91"
                "\x9b\x92\x72\xed\x08\x5a\x3f\xe2\x76\x07\x45"
                "\xac\xa0\xd6\xb6\x02\xf6\xfc\xf9\xe9\xfb\x5d"
                "\x16\xb1\xa1\xf4\xf6\xac\xd4\x9a\x52\x6b\x4a"
                "\xad\x29\xb5\xa6\xd4\x9a\x52\x6b\x4a\xad\x29"
                "\xb5\xa6\xd4\x9a\x52\x6b\x4a\xad\x29\xb5\xa6"
                "\xd4\x9a\x52\x6b\x4a\xad\x29\xb5\xa6\xd4\x9a"
                "\x52\x6b\x4a\xad\x29\xb5\xa6\xd4\x9a\x52\x6b"
                "\x4a\xad\x29\xb5\xa6\xd4\x9a\x52\x6b\xff\x27"
                "\xb5\x66\xab\xb5\x7f\x7e\xad\x96\x1b\xe2\xa1"
                "\xe3\x7e\x63\x1b\xb0\x9f\x36\xf8\x98\x79\x18"
                "\x3a\x3e\x92\x5d\xb8\x05\x27\x28\x84\x0f\x4f"
                "\xab\x77\xe1\x16\x50\xa0\x10\x3e\xec\xd6\x2e"
                "\xc2\x0d\xfc\xc5\xd0\xd1\x06\xec\xc2\x2d\x54"
                "\x4d\x21\x7c\x78\xbc\xb0\x0b\xb7\x40\x0b\x85"
                "\xf0\xe1\x89\xe0\x2e\xdc\x02\xe2\x14\xc2\x87"
                "\xe7\xe9\xef\xcd\xf0\xe5\x2f\x14\xe2\x0f\x00"
            ),

            // packet 4
            sbuf(
                "\xc1\x45"
                "\xec\xda\x31\x0d\x00\x00\x00\xc3\x20\xff\xae"
                "\x67\xa1\xf7\x82\x0e\x68\x0b\x91\x1a\x53\x63"
                "\x6a\x4c\x8d\xa9\x31\x35\xa6\xc6\xd4\x98\x1a"
                "\x53\x63\x6a\x4c\x8d\xa9\x31\x35\xa6\xc6\xd4"
                "\x98\x1a\x53\x63\x6a\x4c\x8d\xa9\x31\x35\xa6"
                "\xc6\xd4\x98\x1a\x53\x63\x6a\x4c\x8d\xa9\xf1"
                "\xa7\x1a\x0f"
            ),
        };

        for (auto const packet : packets) {
            net::write(wss.next_layer(), packet);
            multi_buffer buffer;
            error_code ec;
            wsc.async_read(buffer, [&ec](error_code ec_, std::size_t) { ec = ec_; });
            ioc.run();
            ioc.restart();
            BEAST_EXPECTS(!ec, ec.message());
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
        testIssue1630();
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
