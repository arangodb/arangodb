
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

#include <boost/asio/write.hpp>

#include <boost/config/workaround.hpp>
#if BOOST_WORKAROUND(BOOST_GCC, < 80200)
#define BOOST_BEAST_SYMBOL_HIDDEN __attribute__ ((visibility("hidden")))
#else
#define BOOST_BEAST_SYMBOL_HIDDEN
#endif

namespace boost {
namespace beast {
namespace websocket {

class BOOST_BEAST_SYMBOL_HIDDEN read2_test
    : public websocket_test_suite
{
public:
    template<class Wrap, bool deflateSupported>
    void
    doReadTest(
        Wrap const& w,
        ws_type_t<deflateSupported>& ws,
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

    template<class Wrap, bool deflateSupported>
    void
    doFailTest(
        Wrap const& w,
        ws_type_t<deflateSupported>& ws,
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

    template<bool deflateSupported = true, class Wrap>
    void
    doTestRead(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // already closed
        {
            echo_server es{log};
            stream<test::stream, deflateSupported> ws{ioc_};
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
                    se.code() == net::error::operation_aborted,
                    se.code().message());
            }
        }

        // empty, fragmented message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
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
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            w.write_raw(ws, sbuf(
                "\x01\x81\xff\xff\xff\xff"));
            w.write_raw(ws, sbuf(
                "\xd5"));
            w.write_raw(ws, sbuf(
                "\x80\x81\xff\xff\xff\xff\xd5"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == "**");
        });

        // ping
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                {0x89, 0x00}));
            bool invoked = false;
            ws.control_callback(
                [&](frame_type kind, string_view)
                {
                    BEAST_EXPECT(! invoked);
                    BEAST_EXPECT(kind == frame_type::ping);
                    invoked = true;
                });
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(invoked);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(buffers_to_string(b.data()) == "Hello");
        });

        // ping
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                {0x88, 0x00}));
            bool invoked = false;
            ws.control_callback(
                [&](frame_type kind, string_view)
                {
                    BEAST_EXPECT(! invoked);
                    BEAST_EXPECT(kind == frame_type::close);
                    invoked = true;
                });
            w.write(ws, sbuf("Hello"));
            doReadTest(w, ws, close_code::none);
        });

        // ping then message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            bool once = false;
            ws.control_callback(
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "");
                });
            w.ping(ws, "");
            ws.binary(true);
            w.write(ws, sbuf("Hello"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(ws.got_binary());
            BEAST_EXPECT(buffers_to_string(b.data()) == "Hello");
        });

        // ping then fragmented message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            bool once = false;
            ws.control_callback(
                [&](frame_type kind, string_view s)
                {
                    BEAST_EXPECT(kind == frame_type::pong);
                    BEAST_EXPECT(! once);
                    once = true;
                    BEAST_EXPECT(s == "payload");
                });
            ws.ping("payload");
            w.write_some(ws, false, sbuf("Hello, "));
            w.write_some(ws, false, sbuf(""));
            w.write_some(ws, true, sbuf("World!"));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(once);
            BEAST_EXPECT(buffers_to_string(b.data()) == "Hello, World!");
        });

        // masked message, big
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type_t<deflateSupported> ws{ts};
            ws.next_layer().connect(es.stream());
            ws.set_option(pmd);
            es.async_handshake();
            try
            {
                w.accept(ws);
                std::string const s(2000, '*');
                ws.auto_fragment(false);
                ws.binary(false);
                w.write(ws, net::buffer(s));
                multi_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(ws.got_text());
                BEAST_EXPECT(buffers_to_string(b.data()) == s);
                ws.next_layer().close();
            }
            catch(...)
            {
                ts.close();
                throw;
            }
        });

        // close
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
            stream<test::stream, deflateSupported> ws{ioc, fc};
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
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            w.close(ws, {});
            multi_buffer b;
            doFailTest(w, ws,
                net::error::operation_aborted);
        });

        // buffer overflow
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, net::buffer(s));
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
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            auto const s = std::string(2000, '*') +
                random_string();
            ws.text(true);
            w.write(ws, net::buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // invalid fixed frame header
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            w.write_raw(ws, cbuf(
                {0x8f, 0x80, 0xff, 0xff, 0xff, 0xff}));
            doReadTest(w, ws, close_code::protocol_error);
        });

        // bad close
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                {0x88, 0x02, 0x03, 0xed}));
            doFailTest(w, ws, error::bad_close_code);
        });

        // message size above 2^64
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            w.write_some(ws, false, sbuf("*"));
            w.write_raw(ws, cbuf(
                {0x80, 0xff, 0xff, 0xff, 0xff, 0xff,
                0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff}));
            doReadTest(w, ws, close_code::too_big);
        });

        // message size exceeds max
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.read_message_max(1);
            w.write(ws, sbuf("**"));
            doFailTest(w, ws, error::message_too_big);
        });

        // bad utf8
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            put(ws.next_layer().buffer(), cbuf(
                {0x81, 0x06, 0x03, 0xea, 0xf0, 0x28, 0x8c, 0xbc}));
            doFailTest(w, ws, error::bad_frame_payload);
        });

        // incomplete utf8
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            std::string const s =
                "Hello, world!" "\xc0";
            w.write(ws, net::buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });

        // incomplete utf8, big
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
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
                if(se.code() != error::bad_frame_payload)
                    throw;
            }
        });

        // close frames
        {
            auto const check =
            [&](error_code ev, string_view s)
            {
                echo_server es{log};
                stream<test::stream, deflateSupported> ws{ioc_};
                ws.next_layer().connect(es.stream());
                w.handshake(ws, "localhost", "/");
                ws.next_layer().append(s);
                static_buffer<1> b;
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
            check(error::bad_close_size,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::bad_close_code,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::bad_close_payload,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }
    }

    template<class Wrap>
    void
    doTestReadDeflate(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.client_max_window_bits = 9;
        pmd.server_max_window_bits = 9;
        pmd.compLevel = 1;

        // message size limit
        doTest<true>(pmd,
        [&](ws_type_t<true>& ws)
        {
            std::string const s = std::string(128, '*');
            w.write(ws, net::buffer(s));
            ws.read_message_max(32);
            doFailTest(w, ws, error::message_too_big);
        });

        // invalid inflate block
        doTest<true>(pmd,
        [&](ws_type_t<true>& ws)
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
                if(se.code() == test::error::test_failure)
                    throw;
                BEAST_EXPECTS(se.code().category() ==
                    make_error_code(static_cast<
                        zlib::error>(0)).category(),
                    se.code().message());
            }
            catch(...)
            {
                throw;
            }
        });

        // no_context_takeover
        pmd.server_no_context_takeover = true;
        doTest<true>(pmd,
        [&](ws_type_t<true>& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, net::buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });
        pmd.client_no_context_takeover = false;
    }

    template<class Wrap>
    void
    doTestRead(
        permessage_deflate const& pmd,
        Wrap const& w)
    {
        // message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            ws.auto_fragment(false);
            ws.binary(false);
            w.write(ws, net::buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
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
                w.write(ws, net::buffer(s));
                multi_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(ws.got_text());
                BEAST_EXPECT(buffers_to_string(b.data()) == s);
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
            w.write(ws, net::buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // partial message
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            w.write(ws, net::buffer(s));
            char buf[3];
            auto const bytes_written =
                w.read_some(ws, net::buffer(buf, sizeof(buf)));
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(
                string_view(buf, 3).substr(0, bytes_written) ==
                    s.substr(0, bytes_written));
        });

        // partial message, dynamic buffer
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello, world!";
            w.write(ws, net::buffer(s));
            multi_buffer b;
            auto bytes_written =
                w.read_some(ws, 3, b);
            BEAST_EXPECT(bytes_written > 0);
            BEAST_EXPECT(buffers_to_string(b.data()) ==
                s.substr(0, b.size()));
            w.read_some(ws, 256, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // big message
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, net::buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // message, bad utf8
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "\x03\xea\xf0\x28\x8c\xbc";
            ws.auto_fragment(false);
            ws.text(true);
            w.write(ws, net::buffer(s));
            doReadTest(w, ws, close_code::bad_payload);
        });
    }

    void
    testRead()
    {
        doTestRead<false>(SyncClient{});
        doTestRead<true>(SyncClient{});
        doTestReadDeflate(SyncClient{});
        yield_to([&](yield_context yield)
        {
            doTestRead<false>(AsyncClient{yield});
            doTestRead<true>(AsyncClient{yield});
            doTestReadDeflate(AsyncClient{yield});
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
            check(error::bad_close_size,
                "\x88\x01\x01");

            // invalid close code 1005
            check(error::bad_close_code,
                "\x88\x02\x03\xed");

            // invalid utf8
            check(error::bad_close_payload,
                "\x88\x06\xfc\x15\x0f\xd7\x73\x43");

            // good utf8
            check(error::closed,
                "\x88\x06\xfc\x15utf8");
        }
    }

    void
    run() override
    {
        testRead();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,read2);

} // websocket
} // beast
} // boost
