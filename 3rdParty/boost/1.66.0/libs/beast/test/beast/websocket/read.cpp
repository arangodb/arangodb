//
// Copyright (w) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/websocket/stream.hpp>

#include "test.hpp"

#include <boost/asio/write.hpp>

#include <boost/beast/core/buffers_to_string.hpp>

namespace boost {
namespace beast {
namespace websocket {

class read_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doReadTest(
        Wrap const& w,
        ws_type& ws,
        close_code code)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != error::closed)
                throw;
            BEAST_EXPECT(
                ws.reason().code == code);
        }
    }

    template<class Wrap>
    void
    doFailTest(
        Wrap const& w,
        ws_type& ws,
        error_code ev)
    {
        try
        {
            multi_buffer b;
            w.read(ws, b);
            fail("", __FILE__, __LINE__);
        }
        catch(system_error const& se)
        {
            if(se.code() != ev)
                throw;
        }
    }

    template<class Wrap>
    void
    doTestRead(Wrap const& w)
    {
        using boost::asio::buffer;

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // already closed
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.close({});
            try
            {
                multi_buffer b;
                w.read(ws, b);
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // empty, fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append(
                string_view(
                    "\x01\x00" "\x80\x00", 4));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(b.size() == 0);
        });

        // two part message
        // triggers "fill the read buffer first"
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, sbuf(
                "\x01\x81\xff\xff\xff\xff"));
            w.write_raw(ws, sbuf(
                "\xd5"));
            w.write_raw(ws, sbuf(
                "\x80\x81\xff\xff\xff\xff\xd5"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == "**");
        });

        // ping
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x89, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::ping);
                invoked = true;
            };
            ws.control_callback(cb);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // ping
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x00));
            bool invoked = false;
            auto cb = [&](frame_type kind, string_view)
            {
                BEAST_EXPECT(! invoked);
                BEAST_EXPECT(kind == frame_type::close);
                invoked = true;
            };
            ws.control_callback(cb);
            w.write(ws, sbuf("Hello"));
            doReadTest(w, ws, close_code::none);
        });

        // ping then message
        doTest(pmd, [&](ws_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "");
                };
            ws.control_callback(cb);
            w.ping(ws, "");
            ws.binary(true);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(ws.got_binary());
            BEAST_EXPECT(to_string(b.data()) == "Hello");
        });

        // ping then fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            bool once = false;
            auto cb =
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "payload");
                };
            ws.control_callback(cb);
            ws.ping("payload");
            w.write_some(ws, false, sbuf("Hello, "));
            w.write_some(ws, false, sbuf(""));
            w.write_some(ws, true, sbuf("World!"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(to_string(b.data()) == "Hello, World!");
        });

        // masked message, big
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            ws.set_option(pmd);
            es.async_handshake();
            try
            {
                w.accept(ws);
                std::string const s(2000, '*');
                ws.auto_fragment(false);
                ws.binary(false);
                w.write(ws, buffer(s));
                multi_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(ws.got_text());
                BEAST_EXPECT(to_string(b.data()) == s);
                ws.next_layer().close();
            }
            catch(...)
            {
                ts.close();
                throw;
            }
        });

        // close
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log, kind::async};
            boost::asio::io_context ioc;
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
            ioc.run();
            BEAST_EXPECT(count == 1);
        });

        // already closed
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
            multi_buffer b;
            doFailTest(w, ws,
                boost::asio::error::operation_aborted);
        });

        // buffer overflow
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, buffer(s));
            try
            {
                multi_buffer b(3);
                w.read(ws, b);
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                if(se.code() != error::buffer_overflow)
                    throw;
            }
        });

        // bad utf8, big
        doTest(pmd, [&](ws_type& ws)
        {
            auto const s = std::string(2000, '*') +
                random_string();
            ws.text(true);
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // invalid fixed frame header
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_raw(ws, cbuf(
                0x8f, 0x80, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::protocol_error);
        });

        // bad close
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x88, 0x02, 0x03, 0xed));
            doFailTest(w, ws, error::failed);
        });

        // message size above 2^64
        doTest(pmd, [&](ws_type& ws)
        {
            w.write_some(ws, false, sbuf("*"));
            w.write_raw(ws, cbuf(
                0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff));
            doReadTest(w, ws, close_code::too_big);
        });

        // message size exceeds max
        doTest(pmd, [&](ws_type& ws)
        {
            ws.read_message_max(1);
            w.write(ws, sbuf("**"));
            doFailTest(w, ws, error::failed);
        });

        // bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc));
            doFailTest(w, ws, error::failed);
        });

        // incomplete utf8
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s =
                "Hello, world!" "\xc0";
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // incomplete utf8, big
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s =
                "\x81\x7e\x0f\xa1" +
                std::string(4000, '*') + "\xc0";
            ws.next_layer().append(s);
            multi_buffer b;
            try
            {
                do
                {
                    b.commit(w.read_some(ws, b.prepare(4000)));
                }
                while(! ws.is_message_done());
            }
            catch(system_error const& se)
            {
                if(se.code() != error::failed)
                    throw;
            }
        });

        // close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ioc_};
                ws.next_layer().connect(es.stream());
                w.handshake(ws, "localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                try
                {
                    w.read(ws, b);
                    fail("", __FILE__, __LINE__);
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(se.code() == ev,
                        se.code().message());
                }
                ws.next_layer().close();
            };

            // payload length 1
            check(error::failed,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::failed,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::failed,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }

        //
        // permessage-deflate
        //

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 9;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;

        // message size limit
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = std::string(128, '*');
            w.write(ws, buffer(s));
            ws.read_message_max(32);
            doFailTest(w, ws, error::failed);
        });

        // invalid inflate block
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            ws.next_layer().append(
                "\xc2\x40" + s.substr(0, 64));
            flat_buffer b;
            try
            {
                w.read(ws, b);
            }
            catch(system_error const& se)
            {
                if(se.code() == test::error::fail_error)
                    throw;
                BEAST_EXPECTS(se.code().category() ==
                    zlib::detail::get_error_category(),
                        se.code().message());
            }
            catch(...)
            {
                throw;
            }
        });

        // no_context_takeover
        pmd.server_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });
        pmd.client_no_context_takeover = false;
    }

    template<class Wrap>
    void
    doTestRead(
        permessage_deflate const& pmd,
        Wrap const& w)
    {
        using boost::asio::buffer;

        // message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // masked message
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            ws.set_option(pmd);
            es.async_handshake();
            try
            {
                w.accept(ws);
                std::string const s = "Hello, world!";
                ws.auto_fragment(false);
                ws.binary(false);
                w.write(ws, buffer(s));
                multi_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(ws.got_text());
                BEAST_EXPECT(to_string(b.data()) == s);
                ws.next_layer().close();
            }
            catch(...)
            {
                ts.close();
                throw;
            }
        });

        // empty message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "";
            ws.text(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // partial message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            w.write(ws, buffer(s));
            char buf[3];
            auto const bytes_written =
                w.read_some(ws, buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(
                string_view(buf, 3).substr(0, bytes_written) ==
                    s.substr(0, bytes_written));
        });

        // partial message, dynamic buffer
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            w.write(ws, buffer(s));
            multi_buffer b;
            auto bytes_written =
                w.read_some(ws, 3, b);
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(to_string(b.data()) ==
                s.substr(0, b.size()));
            w.read_some(ws, 256, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // big message
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // message, bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "\x03\xea\xf0\x28\x8c\xbc";
            ws.auto_fragment(false);
            ws.text(true);
            w.write(ws, buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });
    }

    void
    testRead()
    {
        using boost::asio::buffer;

        doTestRead(SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(AsyncClient{yield});
        });

        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;
        doTestRead(pmd, SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(pmd, AsyncClient{yield});
        });

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 9;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;
        doTestRead(pmd, SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead(pmd, AsyncClient{yield});
        });

        // Read close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream> ws{ioc_};
                ws.next_layer().connect(es.stream());
                ws.handshake("localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
                error_code ec;
                ws.read(b, ec);
                BEAST_EXPECTS(ec == ev, ec.message());
                ws.next_layer().close();
            };

            // payload length 1
            check(error::failed,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::failed,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::failed,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }
    }

    void
    testSuspend()
    {
        using boost::asio::buffer;
#if 1
        // suspend on read block
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
            while(! ws.rd_block_)
                ioc.run_one();
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });
#endif

        // suspend on release read block
        doFailLoop([&](test::fail_counter& fc)
        {
//log << "fc.count()==" << fc.count() << std::endl;
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BOOST_ASSERT(ws.rd_block_);
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

#if 1
        // suspend on write pong
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
                    BEAST_EXPECT(to_string(b.data()) == s);
                    ++count;
                });
            BEAST_EXPECT(ws.rd_block_);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    ++count;
                });
            BEAST_EXPECT(ws.wr_block_);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // Ignore ping when closing
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
                    BEAST_EXPECT(to_string(b.data()) == "**");
                    BEAST_EXPECT(++count == 1);
                    b.consume(b.size());
                    ws.async_read(b,
                        [&](error_code ec, std::size_t)
                        {
                            if(ec != boost::asio::error::operation_aborted)
                                BOOST_THROW_EXCEPTION(
                                    system_error{ec});
                            BEAST_EXPECT(++count == 3);
                        });
                });
            BEAST_EXPECT(ws.rd_block_);
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.wr_block_);
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // See if we are already closing
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(ws.rd_block_);
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            BEAST_EXPECT(ws.wr_block_);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });
#endif
    }

    void
    testParseFrame()
    {
        auto const bad =
            [&](string_view s)
            {
                echo_server es{log};
                boost::asio::io_context ioc;
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
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x81\x7e\x01");
            std::size_t count = 0;
            std::string const s(257, '*');
            error_code ec;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == s);
                });
            ioc.run_one();
            es.stream().write_some(
                boost::asio::buffer("\x01" + s));
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
            boost::asio::io_context ioc;
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
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            ws.next_layer().append(
                "\x89\x02*");
            std::size_t count = 0;
            error_code ec;
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    BEAST_EXPECTS(! ec, ec.message());
                    BEAST_EXPECT(to_string(b.data()) == "**");
                });
            ioc.run_one();
            es.stream().write_some(
                boost::asio::buffer(
                    "*" "\x81\x02**"));
            ioc.run();
            BEAST_EXPECT(count == 1);
        }

        // length not canonical
        bad(string_view("\x81\x7e\x00\x7d", 4));
        bad(string_view("\x81\x7f\x00\x00\x00\x00\x00\x00\xff\xff", 10));
    }

    void
    testContHook()
    {
        {
            struct handler
            {
                void operator()(error_code, std::size_t) {}
            };
        
            char buf[32];
            stream<test::stream> ws{ioc_};
            stream<test::stream>::read_some_op<
                boost::asio::mutable_buffer,
                    handler> op{handler{}, ws,
                        boost::asio::mutable_buffer{
                            buf, sizeof(buf)}};
            using boost::asio::asio_handler_is_continuation;
            asio_handler_is_continuation(&op);
            pass();
        }
        {
            struct handler
            {
                void operator()(error_code, std::size_t) {}
            };
        
            multi_buffer b;
            stream<test::stream> ws{ioc_};
            stream<test::stream>::read_op<
                multi_buffer, handler> op{
                    handler{}, ws, b, 32, true};
            using boost::asio::asio_handler_is_continuation;
            asio_handler_is_continuation(&op);
            pass();
        }
    }

    void
    testIssue802()
    {
        for(std::size_t i = 0; i < 100; ++i)
        {
            echo_server es{log, kind::async};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // too-big message frame indicates payload of 2^64-1
            boost::asio::write(ws.next_layer(), sbuf(
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
        boost::asio::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.next_layer().connect(es.stream());
        ws.handshake("localhost", "/");
        ws.write(sbuf("Hello, world!"));
        char buf[4];
        boost::asio::mutable_buffer b{buf, 0};
        auto const n = ws.read_some(b);
        BEAST_EXPECT(n == 0);
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
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            boost::asio::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            boost::asio::io_context ioc;
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
            boost::asio::write(wsc.next_layer(), sbuf(
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
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.set_option(pmd);
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // invalid 1-byte deflate block in frame
            boost::asio::write(ws.next_layer(), sbuf(
                "\xc1\x81\x3a\xa1\x74\x3b\x49"));
        }
#endif
        {
            boost::asio::io_context ioc;
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
            boost::asio::write(wsc.next_layer(), sbuf(
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
            boost::asio::io_context ioc;
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
            boost::asio::write(wsc.next_layer(), sbuf(
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
            boost::asio::io_context ioc;
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
            boost::asio::write(wsc.next_layer(), sbuf(
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
    run() override
    {
        testRead();
        testSuspend();
        testParseFrame();
        testContHook();
        testIssue802();
        testIssue807();
        testIssueBF1();
        testIssueBF2();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read);

} // websocket
} // beast
} // boost
