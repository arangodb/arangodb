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

#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>

namespace boost {
namespace beast {
namespace websocket {

class accept_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestAccept(Wrap const& w)
    {
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

        auto const big = []
        {
            std::string s;
            s += "X1: " + std::string(2000, '*') + "\r\n";
            return s;
        }();

        // request in stream
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            w.accept(ws);
            // VFALCO validate contents of ws.next_layer().str?
        });

        // request in stream, oversized
        {
            stream<test::stream> ws{ioc_,
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
                w.accept(ws);
                fail("", __FILE__, __LINE__);
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
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            bool called = false;
            w.accept_ex(ws, res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in stream, decorator, oversized
        {
            stream<test::stream> ws{ioc_,
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
                w.accept_ex(ws, res_decorator{called});
                fail("", __FILE__, __LINE__);
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
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            w.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"
            ));
        });

        // request in buffers, oversize
        {
            stream<test::stream> ws{ioc_};
            auto tr = connect(ws.next_layer());
            try
            {
                w.accept(ws, boost::asio::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"
                ));
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers, decorator
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            bool called = false;
            w.accept_ex(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n"),
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in buffers, decorator, oversized
        {
            stream<test::stream> ws{ioc_};
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                w.accept_ex(ws, boost::asio::buffer(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                    "Connection: upgrade\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                    "Sec-WebSocket-Version: 13\r\n"
                    + big +
                    "\r\n"),
                    res_decorator{called});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            ts.append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            w.accept(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
            ));
            // VFALCO validate contents of ws.next_layer().str?
        });

        // request in buffers and stream, oversized
        {
            stream<test::stream> ws{ioc_,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                w.accept(ws, sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"
                ));
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in buffers and stream, decorator
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            ts.append(
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(16);
            bool called = false;
            w.accept_ex(ws, sbuf(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"),
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in buffers and stream, decorator, oversize
        {
            stream<test::stream> ws{ioc_,
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                + big +
                "\r\n"};
            auto tr = connect(ws.next_layer());
            try
            {
                bool called = false;
                w.accept_ex(ws, sbuf(
                    "GET / HTTP/1.1\r\n"
                    "Host: localhost\r\n"
                    "Upgrade: websocket\r\n"),
                    res_decorator{called});
                fail("", __FILE__, __LINE__);
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(
                    se.code() == http::error::buffer_overflow,
                    se.code().message());
            }
        }

        // request in message
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            w.accept(ws, req);
        });

        // request in message, decorator
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            bool called = false;
            w.accept_ex(ws, req,
                res_decorator{called});
            BEAST_EXPECT(called);
        });

        // request in message, close frame in stream
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            request_type req;
            req.method(http::verb::get);
            req.target("/");
            req.version(11);
            req.insert(http::field::host, "localhost");
            req.insert(http::field::upgrade, "websocket");
            req.insert(http::field::connection, "upgrade");
            req.insert(http::field::sec_websocket_key, "dGhlIHNhbXBsZSBub25jZQ==");
            req.insert(http::field::sec_websocket_version, "13");
            ts.append("\x88\x82\xff\xff\xff\xff\xfc\x17");
            w.accept(ws, req);
            try
            {
                static_buffer<1> b;
                w.read(ws, b);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if(e.code() != websocket::error::closed)
                    throw;
            }
        });

        // failed handshake (missing Sec-WebSocket-Key)
        doStreamLoop([&](test::stream& ts)
        {
            stream<test::stream&> ws{ts};
            auto tr = connect(ws.next_layer());
            ts.append(
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "Upgrade: websocket\r\n"
                "Connection: upgrade\r\n"
                "Sec-WebSocket-Version: 13\r\n"
                "\r\n");
            ts.read_size(20);
            try
            {
                w.accept(ws);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if( e.code() !=
                        websocket::error::no_sec_key &&
                    e.code() !=
                        boost::asio::error::eof)
                    throw;
            }
        });

        // Closed by client
        {
            stream<test::stream> ws{ioc_};
            auto tr = connect(ws.next_layer());
            tr.close();
            try
            {
                w.accept(ws);
                fail("success", __FILE__, __LINE__);
            }
            catch(system_error const& e)
            {
                if(! BEAST_EXPECTS(
                    e.code() == error::closed,
                    e.code().message()))
                    throw;
            }
        }
    }

    void
    testAccept()
    {
        doTestAccept(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestAccept(AsyncClient{yield});
        });

        //
        // Bad requests
        //

        auto const check =
        [&](error_code const& ev, std::string const& s)
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
                stream<test::stream> ws{ioc_};
                auto tr = connect(ws.next_layer());
                ws.next_layer().append(
                    s.substr(n, s.size() - n));
                try
                {
                    ws.accept(
                        boost::asio::buffer(s.data(), n));
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
    testMoveOnly()
    {
        boost::asio::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_accept(move_only_handler{});
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
        boost::asio::io_context ioc;
        boost::asio::io_service::strand s{ioc};
        stream<test::stream> ws{ioc};
        ws.async_accept(s.wrap(copyable_handler{}));
    }

    void
    run() override
    {
        testAccept();
        testMoveOnly();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,accept);

} // websocket
} // beast
} // boost
