//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/read.hpp>

#include "test_parser.hpp"

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/flat_static_buffer.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/dynamic_body.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <atomic>

namespace boost {
namespace beast {
namespace http {

class read_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    template<bool isRequest>
    void
    failMatrix(char const* s, yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;
        auto const len = strlen(s);
        for(n = 0; n < limit; ++n)
        {
            multi_buffer b;
            b.commit(net::buffer_copy(
                b.prepare(len), net::buffer(s, len)));
            test::fail_count fc(n);
            test::stream ts{ioc_, fc};
            test_parser<isRequest> p(fc);
            error_code ec = test::error::test_failure;
            ts.close_remote();
            read(ts, b, p, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            static std::size_t constexpr pre = 10;
            multi_buffer b;
            b.commit(net::buffer_copy(
                b.prepare(pre), net::buffer(s, pre)));
            test::fail_count fc(n);
            test::stream ts{ioc_, fc,
                std::string(s + pre, len - pre)};
            test_parser<isRequest> p(fc);
            error_code ec = test::error::test_failure;
            ts.close_remote();
            read(ts, b, p, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            multi_buffer b;
            b.commit(net::buffer_copy(
                b.prepare(len), net::buffer(s, len)));
            test::fail_count fc(n);
            test::stream ts{ioc_, fc};
            test_parser<isRequest> p(fc);
            error_code ec = test::error::test_failure;
            ts.close_remote();
            async_read(ts, b, p, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            multi_buffer b;
            b.commit(net::buffer_copy(
                b.prepare(len), net::buffer(s, len)));
            test::fail_count fc(n);
            test::stream ts{ioc_, fc};
            test_parser<isRequest> p(fc);
            error_code ec = test::error::test_failure;
            ts.close_remote();
            async_read_header(ts, b, p, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
        for(n = 0; n < limit; ++n)
        {
            static std::size_t constexpr pre = 10;
            multi_buffer b;
            b.commit(net::buffer_copy(
                b.prepare(pre), net::buffer(s, pre)));
            test::fail_count fc(n);
            test::stream ts(ioc_, fc,
                std::string{s + pre, len - pre});
            test_parser<isRequest> p(fc);
            error_code ec = test::error::test_failure;
            ts.close_remote();
            async_read(ts, b, p, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
    }

    void testThrow()
    {
        try
        {
            multi_buffer b;
            test::stream c{ioc_, "GET / X"};
            c.close_remote();
            request_parser<dynamic_body> p;
            read(c, b, p);
            fail();
        }
        catch(std::exception const&)
        {
            pass();
        }
    }

    void
    testBufferOverflow()
    {
        {
            test::stream c{ioc_};
            ostream(c.buffer()) <<
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "10\r\n"
                "****************\r\n"
                "0\r\n\r\n";
            flat_static_buffer<1024> b;
            request<string_body> req;
            try
            {
                read(c, b, req);
                pass();
            }
            catch(std::exception const& e)
            {
                fail(e.what(), __FILE__, __LINE__);
            }
        }
        {
            test::stream c{ioc_};
            ostream(c.buffer()) <<
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "10\r\n"
                "****************\r\n"
                "0\r\n\r\n";
            error_code ec = test::error::test_failure;
            flat_static_buffer<10> b;
            request<string_body> req;
            read(c, b, req, ec);
            BEAST_EXPECTS(ec == error::buffer_overflow,
                ec.message());
        }
    }

    void testFailures(yield_context do_yield)
    {
        char const* req[] = {
            "GET / HTTP/1.0\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Empty:\r\n"
            "\r\n"
            ,
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Content-Length: 2\r\n"
            "\r\n"
            "**"
            ,
            "GET / HTTP/1.1\r\n"
            "Host: localhost\r\n"
            "User-Agent: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "10\r\n"
            "****************\r\n"
            "0\r\n\r\n"
            ,
            nullptr
        };

        char const* res[] = {
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            ,
            "HTTP/1.0 200 OK\r\n"
            "Server: test\r\n"
            "\r\n"
            "***"
            ,
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 3\r\n"
            "\r\n"
            "***"
            ,
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "10\r\n"
            "****************\r\n"
            "0\r\n\r\n"
            ,
            nullptr
        };
        for(std::size_t i = 0; req[i]; ++i)
            failMatrix<true>(req[i], do_yield);
        for(std::size_t i = 0; res[i]; ++i)
            failMatrix<false>(res[i], do_yield);
    }

    void testRead(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream c{ioc_, fc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            };
            request<dynamic_body> m;
            try
            {
                multi_buffer b;
                read(c, b, m);
                break;
            }
            catch(std::exception const&)
            {
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream ts{ioc_, fc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            };
            request<dynamic_body> m;
            error_code ec = test::error::test_failure;
            multi_buffer b;
            read(ts, b, m, ec);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream c{ioc_, fc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            };
            request<dynamic_body> m;
            error_code ec = test::error::test_failure;
            multi_buffer b;
            async_read(c, b, m, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc{n};
            test::stream c{ioc_, fc,
                "GET / HTTP/1.1\r\n"
                "Host: localhost\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 0\r\n"
                "\r\n"
            };
            request_parser<dynamic_body> m;
            error_code ec = test::error::test_failure;
            multi_buffer b;
            async_read_some(c, b, m, do_yield[ec]);
            if(! ec)
                break;
        }
        BEAST_EXPECT(n < limit);
    }

    void
    testEof(yield_context do_yield)
    {
        {
            multi_buffer b;
            test::stream ts{ioc_};
            request_parser<dynamic_body> p;
            error_code ec;
            ts.close_remote();
            read(ts, b, p, ec);
            BEAST_EXPECT(ec == http::error::end_of_stream);
        }
        {
            multi_buffer b;
            test::stream ts{ioc_};
            request_parser<dynamic_body> p;
            error_code ec;
            ts.close_remote();
            async_read(ts, b, p, do_yield[ec]);
            BEAST_EXPECT(ec == http::error::end_of_stream);
        }
    }

    // Ensure completion handlers are not leaked
    struct handler
    {
        static std::atomic<std::size_t>&
        count() { static std::atomic<std::size_t> n; return n; }
        handler() { ++count(); }
        ~handler() { --count(); }
        handler(handler const&) { ++count(); }
        void operator()(error_code const&, std::size_t) const {}
    };

    void
    testIoService()
    {
        {
            // Make sure handlers are not destroyed
            // after calling io_context::stop
            net::io_context ioc;
            test::stream ts{ioc,
                "GET / HTTP/1.1\r\n\r\n"};
            BEAST_EXPECT(handler::count() == 0);
            multi_buffer b;
            request<dynamic_body> m;
            async_read(ts, b, m, handler{});
            BEAST_EXPECT(handler::count() > 0);
            ioc.stop();
            BEAST_EXPECT(handler::count() > 0);
            ioc.restart();
            BEAST_EXPECT(handler::count() > 0);
            ioc.run_one();
            BEAST_EXPECT(handler::count() == 0);
        }
        {
            // Make sure uninvoked handlers are
            // destroyed when calling ~io_context
            {
                net::io_context ioc;
                test::stream ts{ioc,
                    "GET / HTTP/1.1\r\n\r\n"};
                BEAST_EXPECT(handler::count() == 0);
                multi_buffer b;
                request<dynamic_body> m;
                async_read(ts, b, m, handler{});
                BEAST_EXPECT(handler::count() > 0);
            }
            BEAST_EXPECT(handler::count() == 0);
        }
    }

    // https://github.com/boostorg/beast/issues/430
    void
    testRegression430()
    {
        test::stream ts{ioc_};
        ts.read_size(1);
        ostream(ts.buffer()) <<
          "HTTP/1.1 200 OK\r\n"
          "Transfer-Encoding: chunked\r\n"
          "Content-Type: application/octet-stream\r\n"
          "\r\n"
          "4\r\nabcd\r\n"
          "0\r\n\r\n";
        error_code ec;
        flat_buffer fb;
        response_parser<dynamic_body> p;
        read(ts, fb, p, ec);
        BEAST_EXPECTS(! ec, ec.message());
    }

    //--------------------------------------------------------------------------

    template<class Parser, class Pred>
    void
    readgrind(string_view s, Pred&& pred)
    {
        for(std::size_t n = 1; n < s.size() - 1; ++n)
        {
            Parser p;
            error_code ec = test::error::test_failure;
            flat_buffer b;
            test::stream ts{ioc_};
            ostream(ts.buffer()) << s;
            ts.read_size(n);
            read(ts, b, p, ec);
            if(! BEAST_EXPECTS(! ec, ec.message()))
                continue;
            pred(p);
        }
    }

    void
    testReadGrind()
    {
        readgrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Transfer-Encoding: chunked\r\n"
            "Content-Type: application/octet-stream\r\n"
            "\r\n"
            "4\r\nabcd\r\n"
            "0\r\n\r\n"
            ,[&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.body == "abcd");
            });
        readgrind<test_parser<false>>(
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Expect: Expires, MD5-Fingerprint\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "*****\r\n"
            "2;a;b=1;c=\"2\"\r\n"
            "--\r\n"
            "0;d;e=3;f=\"4\"\r\n"
            "Expires: never\r\n"
            "MD5-Fingerprint: -\r\n"
            "\r\n"
            ,[&](test_parser<false> const& p)
            {
                BEAST_EXPECT(p.body == "*****--");
            });
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
        using strand = net::strand<
            net::io_context::executor_type>;

        // make sure things compile, also can set a
        // breakpoint in asio_handler_invoke to make sure
        // it is instantiated.
        {
            net::io_context ioc;
            strand s{ioc.get_executor()};
            test::stream ts{ioc};
            flat_buffer b;
            request_parser<dynamic_body> p;
            async_read_some(ts, b, p,
                net::bind_executor(
                    s, copyable_handler{}));
        }
        {
            net::io_context ioc;
            strand s{ioc.get_executor()};
            test::stream ts{ioc};
            flat_buffer b;
            request_parser<dynamic_body> p;
            async_read(ts, b, p,
                net::bind_executor(
                    s, copyable_handler{}));
        }
        {
            net::io_context ioc;
            strand s{ioc.get_executor()};
            test::stream ts{ioc};
            flat_buffer b;
            request<dynamic_body> m;
            async_read(ts, b, m,
                net::bind_executor(
                    s, copyable_handler{}));
        }
    }

    void
    run() override
    {
        testThrow();
        testBufferOverflow();

        yield_to([&](yield_context yield)
        {
            testFailures(yield);
        });
        yield_to([&](yield_context yield)
        {
            testRead(yield);
        });
        yield_to([&](yield_context yield)
        {
            testEof(yield);
        });

        testIoService();
        testRegression430();
        testReadGrind();
        testAsioHandlerInvoke();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,read);

} // http
} // beast
} // boost
