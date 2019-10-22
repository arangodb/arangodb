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

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include "test.hpp"

namespace boost {
namespace beast {
namespace websocket {

class write_test : public websocket_test_suite
{
public:
    template<bool deflateSupported, class Wrap>
    void
    doTestWrite(Wrap const& w)
    {
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
                    se.code() == net::error::operation_aborted,
                    se.code().message());
            }
        }

        // message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            std::string const s = "Hello, world!";
            w.write(ws, net::buffer(s));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // empty message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.text(true);
            w.write(ws, net::const_buffer{});
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(b.size() == 0);
        });

        // fragmented message
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.auto_fragment(false);
            ws.binary(false);
            std::string const s = "Hello, world!";
            w.write_some(ws, false, net::buffer(s.data(), 5));
            w.write_some(ws, true, net::buffer(s.data() + 5, s.size() - 5));
            multi_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(ws.got_text());
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // continuation
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            std::string const s = "Hello";
            std::size_t const chop = 3;
            BOOST_ASSERT(chop < s.size());
            w.write_some(ws, false,
                net::buffer(s.data(), chop));
            w.write_some(ws, true, net::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // mask
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.auto_fragment(false);
            std::string const s = "Hello";
            w.write(ws, net::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // mask (large)
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.auto_fragment(false);
            ws.write_buffer_bytes(16);
            std::string const s(32, '*');
            w.write(ws, net::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // mask, autofrag
        doTest<deflateSupported>(pmd,
        [&](ws_type_t<deflateSupported>& ws)
        {
            ws.auto_fragment(true);
            std::string const s(16384, '*');
            w.write(ws, net::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // nomask
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log, kind::async_client};
            ws_type_t<deflateSupported> ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(false);
                std::string const s = "Hello";
                w.write(ws, net::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(buffers_to_string(b.data()) == s);
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
            ws_type_t<deflateSupported> ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                es.async_handshake();
                w.accept(ws);
                ws.auto_fragment(true);
                std::string const s(16384, '*');
                w.write(ws, net::buffer(s));
                flat_buffer b;
                w.read(ws, b);
                BEAST_EXPECT(buffers_to_string(b.data()) == s);
                w.close(ws, {});
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });
    }

    template<class Wrap>
    void
    doTestWriteDeflate(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;
        pmd.compLevel = 1;

        // deflate
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, net::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
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
                net::buffer(s.data(), chop));
            w.write_some(ws, true, net::buffer(
                s.data() + chop, s.size() - chop));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });

        // deflate, no context takeover
        pmd.client_no_context_takeover = true;
        doTest(pmd, [&](ws_type& ws)
        {
            auto const& s = random_string();
            ws.binary(true);
            w.write(ws, net::buffer(s));
            flat_buffer b;
            w.read(ws, b);
            BEAST_EXPECT(buffers_to_string(b.data()) == s);
        });
    }

    void
    testWrite()
    {
        doTestWrite<false>(SyncClient{});
        doTestWrite<true>(SyncClient{});
        doTestWriteDeflate(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestWrite<false>(AsyncClient{yield});
            doTestWrite<true>(AsyncClient{yield});
            doTestWriteDeflate(AsyncClient{yield});
        });
    }

    void
    testPausationAbandoning()
    {
        struct test_op
        {
            std::shared_ptr<stream<test::stream>> s_;

            void operator()(
                boost::system::error_code = {},
                std::size_t = 0)
            {
                BEAST_FAIL();
            }
        };

        std::weak_ptr<stream<test::stream>> weak_ws;
        {
            echo_server es{log};
            net::io_context ioc;
            auto ws = std::make_shared<stream<test::stream>>(ioc);
            test_op op{ws};
            ws->next_layer().connect(es.stream());
            ws->handshake("localhost", "/");
            ws->async_ping("", op);
            BEAST_EXPECT(ws->impl_->wr_block.is_locked());
            ws->async_write(sbuf("*"), op);
            weak_ws = ws;
            ws->next_layer().close();
        }
        BEAST_EXPECT(weak_ws.expired());
    }

    void
    testWriteSuspend()
    {
        // suspend on ping
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
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
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
            ws.async_write(sbuf("*"),
                [&](error_code ec, std::size_t)
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
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log, kind::async_client};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(false);
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log, kind::async_client};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            es.async_handshake();
            ws.accept();
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(false);
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log};
            net::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s(16384, '*');
            ws.auto_fragment(true);
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == 16384);
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
        doFailLoop([&](test::fail_count& fc)
        {
            echo_server es{log, kind::async};
            net::io_context ioc;
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
            ws.async_write(net::buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    ++count;
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                });
            BEAST_EXPECT(ws.impl_->wr_block.is_locked());
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
                net::io_context ioc;
                stream<test::stream> ws{ioc};
                ws.next_layer().connect(es.stream());

                error_code ec;
                ws.handshake("localhost", "/", ec);
                if(! BEAST_EXPECTS(! ec, ec.message()))
                    break;
                ws.async_write_some(false,
                    net::const_buffer{},
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

    void
    testMoveOnly()
    {
        net::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_write_some(
            true, net::const_buffer{},
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
            ws.async_write(net::const_buffer{},
                net::bind_executor(s,
                    copyable_handler{}));
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
            net::io_context ioc;
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

    /*
        https://github.com/boostorg/beast/issues/1666
        
        permessage-deflate not working in version 1.70.
    */
    void
    testIssue1666()
    {
        net::io_context ioc;
        permessage_deflate pmd;
        pmd.client_enable = true;
        pmd.server_enable = true;
        stream<test::stream> ws0{ioc};
        stream<test::stream> ws1{ioc};
        ws0.next_layer().connect(ws1.next_layer());
        ws0.set_option(pmd);
        ws1.set_option(pmd);
        ws1.async_accept(
            [](error_code ec)
            {
                BEAST_EXPECTS(! ec, ec.message());
            });
        ws0.async_handshake("test", "/",
            [](error_code ec)
            {
                BEAST_EXPECTS(! ec, ec.message());
            });
        ioc.run();
        ioc.restart();
        std::string s(256, '*');
        auto const n0 =
            ws0.next_layer().nwrite_bytes();
        error_code ec;
        BEAST_EXPECTS(! ec, ec.message());
        ws1.write(net::buffer(s), ec);
        auto const n1 =
            ws0.next_layer().nwrite_bytes();
        // Make sure the string was actually compressed
        BEAST_EXPECT(n1 < n0 + s.size());
    }

    void
    run() override
    {
        testWrite();
        testPausationAbandoning();
        testWriteSuspend();
        testAsyncWriteFrame();
        testMoveOnly();
        testIssue300();
        testIssue1666();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,write);

} // websocket
} // beast
} // boost
