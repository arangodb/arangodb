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

class close_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestClose(Wrap const& w)
    {
        permessage_deflate pmd;
        pmd.client_enable = false;
        pmd.server_enable = false;

        // close
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {});
        });

        // close with code
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, close_code::going_away);
        });

        // close with code and reason
        doTest(pmd, [&](ws_type& ws)
        {
            w.close(ws, {
                close_code::going_away,
                "going away"});
        });

        // already closed
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            w.close(ws, {});
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == boost::asio::error::operation_aborted,
                    se.code().message());
            }
        }

        // drain a message after close
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x01\x2a");
            w.close(ws, {});
        });

        // drain a big message after close
        {
            std::string s;
            s = "\x81\x7e\x10\x01";
            s.append(4097, '*');
            doTest(pmd, [&](ws_type& ws)
            {
                ws.next_layer().append(s);
                w.close(ws, {});
            });
        }

        // drain a ping after close
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x89\x01*");
            w.close(ws, {});
        });

        // drain invalid message frame after close
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x81\x81\xff\xff\xff\xff*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // drain invalid close frame after close
        {
            echo_server es{log};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            w.handshake(ws, "localhost", "/");
            ws.next_layer().append("\x88\x01*");
            try
            {
                w.close(ws, {});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::failed,
                    se.code().message());
            }
        }

        // drain masked close frame
        {
            echo_server es{log, kind::async_client};
            stream<test::stream> ws{ioc_};
            ws.next_layer().connect(es.stream());
            ws.set_option(pmd);
            es.async_handshake();
            ws.accept();
            w.close(ws, {});
        }

        // close with incomplete read message
        doTest(pmd, [&](ws_type& ws)
        {
            ws.next_layer().append("\x81\x02**");
            static_buffer<1> b;
            w.read_some(ws, 1, b);
            w.close(ws, {});
        });
    }

    void
    testClose()
    {
        doTestClose(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestClose(AsyncClient{yield});
        });
    }

    void
    testSuspend()
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
            ws.async_close({},
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

        // suspend on write
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
            BEAST_EXPECT(ws.wr_block_);
            BEAST_EXPECT(count == 0);
            ws.async_close({},
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
            ws.async_close({},
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
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
                    if(ec != error::failed)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            while(! ws.wr_block_)
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // suspend on read close #1
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
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
                    if(ec != error::closed)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            while(! ws.wr_block_)
            {
                ioc.run_one();
                if(! BEAST_EXPECT(! ioc.stopped()))
                    break;
            }
            BEAST_EXPECT(count == 0);
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 2);
        });

        // teardown on received close
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // add a close frame to the input
            ws.next_layer().append(string_view{
                "\x88\x00", 2});
            std::size_t count = 0;
            std::string const s = "Hello, world!";
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    BEAST_EXPECT(++count == 1);
                });
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 3);
                });
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // check for deadlock
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            // add a ping frame to the input
            ws.next_layer().append(string_view{
                "\x89\x00", 2});
            std::size_t count = 0;
            multi_buffer b;
            std::string const s = "Hello, world!";
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    BEAST_EXPECT(++count == 1);
                });
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 3);
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
            BEAST_EXPECT(ws.is_open());
            BEAST_EXPECT(ws.wr_block_);
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 3);
        });

        // Four-way: close, read, write, ping
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s = "Hello, world!";
            multi_buffer b;
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                });
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                });
            ws.async_ping({},
                [&](error_code ec)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 4);
        });

        // Four-way: read, write, ping, close
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s = "Hello, world!";
            multi_buffer b;
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec && ec != boost::asio::error::operation_aborted)
                    {
                        BEAST_EXPECTS(ec, ec.message());
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    }
                    if(! ec)
                        BEAST_EXPECT(to_string(b.data()) == s);
                    ++count;
                    if(count == 4)
                        BEAST_EXPECT(
                            ec == boost::asio::error::operation_aborted);
                });
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t n)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(n == s.size());
                    BEAST_EXPECT(++count == 1);
                });
            ws.async_ping({},
                [&](error_code ec)
                {
                    if(ec != boost::asio::error::operation_aborted)
                    {
                        BEAST_EXPECTS(ec, ec.message());
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    }
                    ++count;
                });
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                    BEAST_EXPECT(count == 2 || count == 3);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 4);
        });

        // Four-way: ping, read, write, close
        doFailLoop([&](test::fail_counter& fc)
        {
            echo_server es{log};
            boost::asio::io_context ioc;
            stream<test::stream> ws{ioc, fc};
            ws.next_layer().connect(es.stream());
            ws.handshake("localhost", "/");
            std::size_t count = 0;
            std::string const s = "Hello, world!";
            multi_buffer b;
            ws.async_ping({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 1);
                });
            ws.async_read(b,
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                });
            ws.async_write(buffer(s),
                [&](error_code ec, std::size_t)
                {
                    if(ec != boost::asio::error::operation_aborted)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    ++count;
                });
            ws.async_close({},
                [&](error_code ec)
                {
                    if(ec)
                        BOOST_THROW_EXCEPTION(
                            system_error{ec});
                    BEAST_EXPECT(++count == 2);
                });
            BEAST_EXPECT(count == 0);
            ioc.run();
            BEAST_EXPECT(count == 4);
        });
    }

    void
    testContHook()
    {
        struct handler
        {
            void operator()(error_code) {}
        };

        stream<test::stream> ws{ioc_};
        stream<test::stream>::close_op<handler> op{
            handler{}, ws, {}};
        using boost::asio::asio_handler_is_continuation;
        asio_handler_is_continuation(&op);
    }

    void
    run() override
    {
        testClose();
        testSuspend();
        testContHook();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,close);

} // websocket
} // beast
} // boost
