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

namespace boost {
namespace beast {
namespace websocket {

class write_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestWrite(Wrap const& w)
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
                w.write(ws, sbuf(""));
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            std::string const s = "Hello, world!";
            w.write(ws, buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // empty message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.text(true);
            w.write(ws, boost::asio::null_buffers{});
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(b.size() == 0);
        });

        // fragmented message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            std::string const s = "Hello, world!";
            w.write_some(ws, false, buffer(s.data(), 5));
            w.write_some(ws, true, buffer(s.data() + 5, s.size() - 5));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            w.write_some(ws, false,
                buffer(s.data(), chop));
            w.write_some(ws, true, buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            std::string const s = "Hello";
            w.write(ws, buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask (large)
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(false);
            ws.write_buffer_size(16);
            std::string const s(32, '*');
            w.write(ws, buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // mask, autofrag
        doTest(pmd, [&](ws_type& ws)
        {
            ws.auto_fragment(true);
            std::string const s(16384, '*');
            w.write(ws, buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // nomask
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(false);
                std::string const s = "Hello";
                w.write(ws, buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // nomask, autofrag
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(true);
                std::string const s(16384, '*');
                w.write(ws, buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.compLevel = 1;

        // deflate
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, continuation
        doTest(pmd, [&](ws_type& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            // This call should produce no
            // output due to compression latency.
            w.write_some(ws, false,
                buffer(s.data(), chop));
            w.write_some(ws, true, buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });

        // deflate, no context takeover
        pmd.client_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(to_string(b.data()) == s);
        });
    }

    void
    testWrite()
    {
        using boost::asio::buffer;

        doTestWrite(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestWrite(AsyncClient{yield});
        });
    }

    void
    testWriteSuspend()
    {
        using boost::asio::buffer;

        // suspend on ping
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(ws.wr_block_);
            BEAST_EXPECT(count == 0);
            ws.async_write(sbuf("*"),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 1);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on close
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
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(ws.wr_block_);
            BEAST_EXPECT(count == 0);
            ws.async_write(sbuf("*"),
                [&](error_code ec, std::size_t)
                {
                    ++count;
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read ping + message
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
            while(! ws.wr_block_)
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_write(sbuf("*"),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 1);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on ping: nomask, nofrag
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log, kind::async_client};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(false);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on ping: nomask, frag
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log, kind::async_client};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on ping: mask, nofrag
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            error_code ec;
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(false);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
        });

        // suspend on ping: mask, frag
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            error_code ec;
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
        });

        // suspend on ping: deflate
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log, kind::async};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            {
                permessage_deflate pmd;
                pmd.client_enable = true;
                pmd.compLevel = 1;
                ws.set_option(pmd);
            }
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            auto const& s = random_string();
            ws.binary(true);
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                });
            BEAST_EXPECT(ws.wr_block_);
            ws.async_ping("",
                [&](error_code ec)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                });
            ioc.run();
        });
    }

    void
    testAsyncWriteFrame()
    {
        for(int i = 0; i < 2; ++i)
        {
            for(;;)
            {
                echo_server es{log, i==1 ?
                    kind::async : kind::sync};
                boost::asio::io_context ioc;
                stream<test::stream> ws{ioc};
                ws.next_layer().connect(es.stream());

                error_code ec;
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.async_write_some(false,
                    boost::asio::null_buffers{},
                    [&](error_code, std::size_t)
                    {
                        fail();
                    });
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                //
                // Destruction of the io_context will cause destruction
                // of the write_some_op without invoking the final handler.
                //
                break;
            }
        }
    }

    /*
        https://github.com/boostorg/beast/issues/300

        Write a message as two individual frames
    */
    void
    testIssue300()
    {
        for(int i = 0; i < 2; ++i )
        {
            echo_server es{log, i==1 ?
                kind::async : kind::sync};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc};
            ws.next_layer().connect(es.stream());

            error_code ec;
            ws.handshake("localhost", "/", ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                return;
            ws.write_some(false, sbuf("u"));
            ws.write_some(true, sbuf("v"));
            multi_buffer b;
            ws.read(b, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
    }

    void
    testContHook()
    {
        struct handler
        {
            void operator()(error_code) {}
        };
        
        char buf[32];
        stream<test::stream> ws{ioc_};
        stream<test::stream>::write_some_op<
            boost::asio::const_buffer,
                handler> op{handler{}, ws, true,
                    boost::asio::const_buffer{
                        buf, sizeof(buf)}};
        using boost::asio::asio_handler_is_continuation;
        asio_handler_is_continuation(&op);
    }

    void
    run() override
    {
        testWrite();
        testWriteSuspend();
        testAsyncWriteFrame();
        testIssue300();
        testContHook();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,write);

} // websocket
} // beast
} // boost
