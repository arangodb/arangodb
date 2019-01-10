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

class handshake_test : public websocket_test_suite
{
public:
    template<class Wrap>
    void
    doTestHandshake(Wrap const& w)
    {
        class req_decorator
        {
            bool& b_;

        public:
            req_decorator(req_decorator const&) = default;

            explicit
            req_decorator(bool& b)
                : b_(b)
            {
            }

            void
            operator()(request_type&) const
            {
                b_ = true;
            }
        };

        // handshake
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            try
            {
                w.handshake(ws, "localhost", "/");
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            response_type res;
            try
            {
                w.handshake(ws, res, "localhost", "/");
                // VFALCO validate res?
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, decorator
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            try
            {
                w.handshake_ex(ws, "localhost", "/",
                    req_decorator{called});
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });

        // handshake, response, decorator
        doStreamLoop([&](test::stream& ts)
        {
            echo_server es{log};
            ws_type ws{ts};
            ws.next_layer().connect(es.stream());
            bool called = false;
            response_type res;
            try
            {
                w.handshake_ex(ws, res, "localhost", "/",
                    req_decorator{called});
                // VFALCO validate res?
                BEAST_EXPECT(called);
            }
            catch(...)
            {
                ts.close();
                throw;
            }
            ts.close();
        });
    }

    void
    testHandshake()
    {
        doTestHandshake(SyncClient{});

        yield_to([&](yield_context yield)
        {
            doTestHandshake(AsyncClient{yield});
        });

        auto const check =
        [&](error e, std::string const& s)
        {
            stream<test::stream> ws{ioc_};
            auto tr = connect(ws.next_layer());
            ws.next_layer().append(s);
            tr.close();
            try
            {
                ws.handshake("localhost:80", "/");
                fail();
            }
            catch(system_error const& se)
            {
                BEAST_EXPECTS(se.code() == e, se.what());
            }
        };
        // bad HTTP version
        check(error::bad_http_version,
            "HTTP/1.0 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // no Connection
        check(error::no_connection,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // no Connection upgrade
        check(error::no_connection_upgrade,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: keep-alive\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // no Upgrade
        check(error::no_upgrade,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // no Upgrade websocket
        check(error::no_upgrade_websocket,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: HTTP/2\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // no Sec-WebSocket-Accept
        check(error::no_sec_accept,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // bad Sec-WebSocket-Accept
        check(error::bad_sec_accept,
            "HTTP/1.1 101 Switching Protocols\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: *\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
        // declined
        check(error::upgrade_declined,
            "HTTP/1.1 200 OK\r\n"
            "Server: beast\r\n"
            "Upgrade: WebSocket\r\n"
            "Connection: upgrade\r\n"
            "Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "\r\n"
        );
    }

    // Compression Extensions for WebSocket
    //
    // https://tools.ietf.org/html/rfc7692
    //
    void
    testExtRead()
    {
        detail::pmd_offer po;

        auto const accept =
        [&](string_view s)
        {
            http::fields f;
            f.set(http::field::sec_websocket_extensions, s);
            po = detail::pmd_offer();
            detail::pmd_read(po, f);
            BEAST_EXPECT(po.accept);
        };

        auto const reject =
        [&](string_view s)
        {
            http::fields f;
            f.set(http::field::sec_websocket_extensions, s);
            po = detail::pmd_offer();
            detail::pmd_read(po, f);
            BEAST_EXPECT(! po.accept);
        };

        // duplicate parameters
        reject("permessage-deflate; server_max_window_bits=8; server_max_window_bits=8");

        // missing value
        reject("permessage-deflate; server_max_window_bits");
        reject("permessage-deflate; server_max_window_bits=");

        // invalid value
        reject("permessage-deflate; server_max_window_bits=-1");
        reject("permessage-deflate; server_max_window_bits=7");
        reject("permessage-deflate; server_max_window_bits=16");
        reject("permessage-deflate; server_max_window_bits=999999999999999999999999");
        reject("permessage-deflate; server_max_window_bits=9a");

        // duplicate parameters
        reject("permessage-deflate; client_max_window_bits=8; client_max_window_bits=8");

        // optional value excluded
        accept("permessage-deflate; client_max_window_bits");
        BEAST_EXPECT(po.client_max_window_bits == -1);
        accept("permessage-deflate; client_max_window_bits=");
        BEAST_EXPECT(po.client_max_window_bits == -1);

        // invalid value
        reject("permessage-deflate; client_max_window_bits=-1");
        reject("permessage-deflate; client_max_window_bits=7");
        reject("permessage-deflate; client_max_window_bits=16");
        reject("permessage-deflate; client_max_window_bits=999999999999999999999999");

        // duplicate parameters
        reject("permessage-deflate; server_no_context_takeover; server_no_context_takeover");

        // valueless parameter
        accept("permessage-deflate; server_no_context_takeover");
        BEAST_EXPECT(po.server_no_context_takeover);
        accept("permessage-deflate; server_no_context_takeover=");
        BEAST_EXPECT(po.server_no_context_takeover);

        // disallowed value
        reject("permessage-deflate; server_no_context_takeover=-1");
        reject("permessage-deflate; server_no_context_takeover=x");
        reject("permessage-deflate; server_no_context_takeover=\"yz\"");
        reject("permessage-deflate; server_no_context_takeover=999999999999999999999999");

        // duplicate parameters
        reject("permessage-deflate; client_no_context_takeover; client_no_context_takeover");

        // valueless parameter
        accept("permessage-deflate; client_no_context_takeover");
        BEAST_EXPECT(po.client_no_context_takeover);
        accept("permessage-deflate; client_no_context_takeover=");
        BEAST_EXPECT(po.client_no_context_takeover);

        // disallowed value
        reject("permessage-deflate; client_no_context_takeover=-1");
        reject("permessage-deflate; client_no_context_takeover=x");
        reject("permessage-deflate; client_no_context_takeover=\"yz\"");
        reject("permessage-deflate; client_no_context_takeover=999999999999999999999999");

        // unknown extension parameter
        reject("permessage-deflate; unknown");
        reject("permessage-deflate; unknown=");
        reject("permessage-deflate; unknown=1");
        reject("permessage-deflate; unknown=x");
        reject("permessage-deflate; unknown=\"xy\"");
    }

    void
    testExtWrite()
    {
        detail::pmd_offer po;

        auto const check =
        [&](string_view match)
        {
            http::fields f;
            detail::pmd_write(f, po);
            BEAST_EXPECT(
                f[http::field::sec_websocket_extensions]
                    == match);
        };

        po.accept = true;
        po.server_max_window_bits = 0;
        po.client_max_window_bits = 0;
        po.server_no_context_takeover = false;
        po.client_no_context_takeover = false;
        
        check("permessage-deflate");

        po.server_max_window_bits = 10;
        check("permessage-deflate; server_max_window_bits=10");

        po.server_max_window_bits = -1;
        check("permessage-deflate; server_max_window_bits");

        po.server_max_window_bits = 0;
        po.client_max_window_bits = 10;
        check("permessage-deflate; client_max_window_bits=10");

        po.client_max_window_bits = -1;
        check("permessage-deflate; client_max_window_bits");

        po.client_max_window_bits = 0;
        po.server_no_context_takeover = true;
        check("permessage-deflate; server_no_context_takeover");

        po.server_no_context_takeover = false;
        po.client_no_context_takeover = true;
        check("permessage-deflate; client_no_context_takeover");
    }

    void
    testExtNegotiate()
    {
        permessage_deflate pmd;

        auto const reject =
        [&](
            string_view offer)
        {
            detail::pmd_offer po;
            {
                http::fields f;
                f.set(http::field::sec_websocket_extensions, offer);
                detail::pmd_read(po, f);
            }
            http::fields f;
            detail::pmd_offer config;
            detail::pmd_negotiate(f, config, po, pmd);
            BEAST_EXPECT(! config.accept);
        };

        auto const accept =
        [&](
            string_view offer,
            string_view result)
        {
            detail::pmd_offer po;
            {
                http::fields f;
                f.set(http::field::sec_websocket_extensions, offer);
                detail::pmd_read(po, f);
            }
            http::fields f;
            detail::pmd_offer config;
            detail::pmd_negotiate(f, config, po, pmd);
            auto const got =
                f[http::field::sec_websocket_extensions];
            BEAST_EXPECTS(got == result, got);
            {
                detail::pmd_offer poc;
                detail::pmd_read(poc, f);
                detail::pmd_normalize(poc);
                BEAST_EXPECT(poc.accept);
            }
            BEAST_EXPECT(config.server_max_window_bits != 0);
            BEAST_EXPECT(config.client_max_window_bits != 0);
        };

        pmd.server_enable = true;
        pmd.server_max_window_bits = 15;
        pmd.client_max_window_bits = 15;
        pmd.server_no_context_takeover = false;
        pmd.client_no_context_takeover = false;

        // default
        accept(
            "permessage-deflate",
            "permessage-deflate");

        // non-default server_max_window_bits
        accept(
            "permessage-deflate; server_max_window_bits=14",
            "permessage-deflate; server_max_window_bits=14");

        // explicit default server_max_window_bits
        accept(
            "permessage-deflate; server_max_window_bits=15",
            "permessage-deflate");

        // minimum window size of 8 bits (a zlib bug)
        accept(
            "permessage-deflate; server_max_window_bits=8",
            "permessage-deflate; server_max_window_bits=9");

        // non-default server_max_window_bits setting
        pmd.server_max_window_bits = 10;
        accept(
            "permessage-deflate",
            "permessage-deflate; server_max_window_bits=10");

        // clamped server_max_window_bits setting #1
        pmd.server_max_window_bits = 10;
        accept(
            "permessage-deflate; server_max_window_bits=14",
            "permessage-deflate; server_max_window_bits=10");

        // clamped server_max_window_bits setting #2
        pmd.server_max_window_bits=8;
        accept(
            "permessage-deflate; server_max_window_bits=14",
            "permessage-deflate; server_max_window_bits=9");

        pmd.server_max_window_bits = 15;

        // present with no value
        accept(
            "permessage-deflate; client_max_window_bits",
            "permessage-deflate");

        // present with no value, non-default setting
        pmd.client_max_window_bits = 10;
        accept(
            "permessage-deflate; client_max_window_bits",
            "permessage-deflate; client_max_window_bits=10");

        // absent, non-default setting
        pmd.client_max_window_bits = 10;
        reject(
            "permessage-deflate");
    }

    void
    testMoveOnly()
    {
        boost::asio::io_context ioc;
        stream<test::stream> ws{ioc};
        ws.async_handshake("", "", move_only_handler{});
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
        ws.async_handshake("localhost", "/", s.wrap(copyable_handler{}));
    }

    void
    run() override
    {
        testHandshake();
        testExtRead();
        testExtWrite();
        testExtNegotiate();
        testMoveOnly();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,handshake);

} // websocket
} // beast
} // boost
