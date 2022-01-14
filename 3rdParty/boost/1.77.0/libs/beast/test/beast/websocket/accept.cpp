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
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include "test.hpp"
#if BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/use_awaitable.hpp>
#endif
namespace boost {
namespace beast {
namespace websocket {

class accept_test : public unit_test::suite //: public websocket_test_suite
{
public:
    class res_decorator
    {
        bool& b_;

    public:
        res_decorator(res_decorator const&) = default;

        explicit
        res_decorator(bool& b)
            : b_(b)
        {
        }

        void
        operator()(response_type&) const
        {
            this->b_ = true;
        }
    };

    template<std::size_t N>
    static
    net::const_buffer
    sbuf(const char (&s)[N])
    {
        return net::const_buffer(&s[0], N-1);
    }

    static
    void
    fail_loop(
        std::function<void(
            stream<test::basic_stream<net::io_context::executor_type>>&)>
                f,
        std::chrono::steady_clock::duration amount =
            std::chrono::seconds(5))
    {
        using clock_type = std::chrono::steady_clock;
        auto const expires_at =
            clock_type::now() + amount;
        net::io_context ioc;
        for(std::size_t n = 0;;++n)
        {
            test::fail_count fc(n);
            try
            {
                stream<test::basic_stream<net::io_context::executor_type>> 
                    ws(ioc, fc);
                auto tr = connect(ws.next_layer());
                f(ws);
                break;
            }
            catch(system_error const& se)
            {
                // VFALCO Commented this out after the short
                // read change, because it converts test_failure
                // into http::partial_message
                //
                boost::ignore_unused(se);
            #if 0
                if(! BEAST_EXPECTS(
                    se.code() == test::error::test_failure,
                    se.code().message()))
                    throw;
            #endif
                if(! BEAST_EXPECTS(
                    clock_type::now() < expires_at,
                    "a test timeout occurred"))
                    break;
            }
        }
    }

    template<class Api>
    void
    testMatrix(Api api)
    {
        net::io_context ioc;

        // request in stream
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            ws.next_layer().append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ws.next_layer().read_size(20);
            api.accept(ws);
        });

        // request in stream, decorator
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            ws.next_layer().append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ws.next_layer().read_size(20);
            bool called = false;
            ws.set_option(stream_base::decorator(
                res_decorator{called}));
            api.accept(ws);
            BEAST_EXPECT(called);
        });

        // request in buffers
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            api.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"
            ));
        });

        // request in buffers, decorator
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            bool called = false;
            ws.set_option(stream_base::decorator(
                res_decorator{called}));
            api.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"));
            BEAST_EXPECT(called);
        });

        // request in buffers and stream
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            ws.next_layer().append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ws.next_layer().read_size(16);
            api.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
            ));
            // VFALCO validate contents of ws.next_layer().str?
        });

        // request in buffers and stream, decorator
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            ws.next_layer().append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ws.next_layer().read_size(16);
            bool called = false;
            ws.set_option(stream_base::decorator(
                res_decorator{called}));
            api.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"));
            BEAST_EXPECT(called);
        });

        // request in message
        {
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");

            fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
            {
                api.accept(ws, req);
            });
        }

        // request in message, decorator
        {
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");

            fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
            {
                bool called = false;
                ws.set_option(stream_base::decorator(
                    res_decorator{called}));
                api.accept(ws, req);
                BEAST_EXPECT(called);
            });
        }

        // request in message, close frame in stream
        {
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");

            fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
            {
                ws.next_layer().append("\x88\x82\xff\xff\xff\xff\xfc\x17");
                api.accept(ws, req);
                try
                {
                    static_buffer<1> b;
                    api.read(ws, b);
                    fail("success", __FILE__, __LINE__);
                }
                catch(system_error const& e)
                {
                    if(e.code() != websocket::error::closed)
                        throw;
                }
            });
        }

        // failed handshake (missing Sec-WebSocket-Key)
        fail_loop([&](stream<test::basic_stream<net::io_context::executor_type>>& ws)
        {
            ws.next_layer().append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ws.next_layer().read_size(20);
            try
            {
                api.accept(ws);
                BEAST_FAIL();
            }
            catch(system_error const& e)
            {
                if( e.code() != websocket::error::no_sec_key &&
                    e.code() != net::error::eof)
                    throw;
            }
        });
    }

    template<class Api>
    void
    testOversized(Api const& api)
    {
        net::io_context ioc;

        auto const big = []
        {
            std::string s;
            s += "X1: " + std::string(2000, '*') + "\r\n";
            return s;
        }();

        // request in stream
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                api.accept(ws);
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                // VFALCO Its the http error category...
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in stream, decorator
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                ws.set_option(stream_base::decorator(
                    res_decorator{called}));
                api.accept(ws);
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                // VFALCO Its the http error category...
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc};
            auto tr = connect(ws.next_layer());
            try
            {
                api.accept(ws, net::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"
                ));
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers, decorator
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc};
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                ws.set_option(stream_base::decorator(
                    res_decorator{called}));
                api.accept(ws, net::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"));
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                api.accept(ws, websocket_test_suite::sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                ));
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream, decorator
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws{ioc,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                ws.set_option(stream_base::decorator(
                    res_decorator{called}));
                api.accept(ws, websocket_test_suite::sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"));
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }
    }

    void
    testInvalidInputs()
    {
        net::io_context ioc;

        auto const check =
        [&](error_code ev, string_view s)
        {
            for(int i = 0; i < 3; ++i)
            {
                std::size_t n;
                switch(i)
                {
                default:
                case 0:
                    n = 1;
                    break;
                case 1:
                    n = s.size() / 2;
                    break;
                case 2:
                    n = s.size() - 1;
                    break;
                }
                stream<test::basic_stream<net::io_context::executor_type>> ws(ioc);
                auto tr = connect(ws.next_layer());
                ws.next_layer().append(
                    s.substr(n, s.size() - n));
                tr.close();
                try
                {
                    ws.accept(net::buffer(s.data(), n));
                    BEAST_EXPECTS(! ev, ev.message());
                }
                catch(system_error const& se)
                {
                    BEAST_EXPECTS(se.code() == ev, se.what());
                }
            }
        };

        // bad version
        check(error::bad_http_version,
            "GET / HTTP/1.0\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // bad method
        check(error::bad_method,
            "POST / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Host
        check(error::no_host,
            "GET / HTTP/1.1\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Connection
        check(error::no_connection,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Connection upgrade
        check(error::no_connection_upgrade,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Upgrade
        check(error::no_upgrade,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Upgrade websocket
        check(error::no_upgrade_websocket,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: HTTP/2\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Sec-WebSocket-Key
        check(error::no_sec_key,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // bad Sec-WebSocket-Key
        check(error::bad_sec_key,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQdGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );

        // no Sec-WebSocket-Version
        check(error::no_sec_version,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "\r\n"
        );

        // bad Sec-WebSocket-Version
        check(error::bad_sec_version,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive,upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 1\r\n"
            "\r\n"
        );

        // bad Sec-WebSocket-Version
        check(error::bad_sec_version,
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 12\r\n"
            "\r\n"
        );

        // valid request
        check({},
            "GET / HTTP/1.1\r\n"
            "Host: localhost:80\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
    }

    void
    testEndOfStream()
    {
        net::io_context ioc;
        {
            stream<test::basic_stream<net::io_context::executor_type>> ws(ioc);
            auto tr = connect(ws.next_layer());
            tr.close();
            try
            {
                test_sync_api api;
                api.accept(ws, net::const_buffer{});
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::closed,
                    se.code().message());
            }
        }
        {
            stream<test::basic_stream<net::io_context::executor_type>>
                ws(ioc.get_executor());
            auto tr = connect(ws.next_layer());
            tr.close();
            try
            {
                test_async_api api;
                api.accept(ws, net::const_buffer{});
                BEAST_FAIL();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::closed,
                    se.code().message());
            }
        }
    }

    void
    testAsync()
    {
        using tcp = net::ip::tcp;

        net::io_context ioc;

        // success, no timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run_for(ioc, std::chrono::seconds(1));
        }

        {
            stream<test::basic_stream<net::io_context::executor_type>> ws1(ioc);
            stream<test::basic_stream<net::io_context::executor_type>> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.async_handshake("test", "/", test::success_handler());
            ws2.async_accept(test::success_handler());
            test::run_for(ioc, std::chrono::seconds(1));
        }

        // success, timeout enabled

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(stream_base::timeout{
                std::chrono::milliseconds(50),
                stream_base::none(),
                false});
            ws1.async_accept(test::success_handler());
            ws2.async_handshake("test", "/", test::success_handler());
            test::run_for(ioc, std::chrono::seconds(1));
        }

        {
            stream<test::basic_stream<net::io_context::executor_type>> ws1(ioc);
            stream<test::basic_stream<net::io_context::executor_type>> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(stream_base::timeout{
                std::chrono::milliseconds(50),
                stream_base::none(),
                false});
            ws1.async_accept(test::success_handler());
            ws2.async_handshake("test", "/", test::success_handler());
            test::run_for(ioc, std::chrono::seconds(1));
        }

        // timeout

        {
            stream<tcp::socket> ws1(ioc);
            stream<tcp::socket> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(stream_base::timeout{
                std::chrono::milliseconds(50),
                stream_base::none(),
                false});
            ws1.async_accept(test::fail_handler(beast::error::timeout));
            test::run_for(ioc, std::chrono::seconds(1));
        }

        {
            stream<test::basic_stream<net::io_context::executor_type>> ws1(ioc);
            stream<test::basic_stream<net::io_context::executor_type>> ws2(ioc);
            test::connect(ws1.next_layer(), ws2.next_layer());

            ws1.set_option(stream_base::timeout{
                std::chrono::milliseconds(50),
                stream_base::none(),
                false});
            ws1.async_accept(test::fail_handler(beast::error::timeout));
            test::run_for(ioc, std::chrono::seconds(1));
        }

        // abandoned operation

        {
            {
                stream<tcp::socket> ws1(ioc);
                ws1.async_accept(test::fail_handler(
                    net::error::operation_aborted));
            }
            test::run(ioc);
        }

        {
            {
                stream<tcp::socket> ws1(ioc);
                string_view s =
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    "\r\n";
                error_code ec;
                http::request_parser<http::empty_body> p;
                p.put(net::const_buffer(s.data(), s.size()), ec);
                ws1.async_accept(p.get(), test::fail_handler(
                    net::error::operation_aborted));
            }
            test::run(ioc);
        }
    }

#if BOOST_ASIO_HAS_CO_AWAIT
    void testAwaitableCompiles(
        stream<net::ip::tcp::socket>& s,
        http::request<http::empty_body>& req,
        net::mutable_buffer buf
        )
    {
        static_assert(std::is_same_v<
            net::awaitable<void>, decltype(
            s.async_accept(net::use_awaitable))>);

        static_assert(std::is_same_v<
            net::awaitable<void>, decltype(
            s.async_accept(req, net::use_awaitable))>);

        static_assert(std::is_same_v<
            net::awaitable<void>, decltype(
            s.async_accept(buf, net::use_awaitable))>);
    }
#endif

    void
    run() override
    {
        testMatrix(test_sync_api{});
        testMatrix(test_async_api{});
        testOversized(test_sync_api{});
        testOversized(test_async_api{});
        testInvalidInputs();
        testEndOfStream();
        testAsync();
#if BOOST_ASIO_HAS_CO_AWAIT
        boost::ignore_unused(&accept_test::testAwaitableCompiles);
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,accept);

} // websocket
} // beast
} // boost
