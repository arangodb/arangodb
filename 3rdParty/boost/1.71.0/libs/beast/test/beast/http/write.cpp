//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/write.hpp>

#include <boost/beast/http/buffer_body.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <sstream>
#include <string>

namespace boost {
namespace beast {
namespace http {

class write_test
    : public beast::unit_test::suite
    , public test::enable_yield_to
{
public:
    struct unsized_body
    {
        using value_type = std::string;

        class writer
        {
            value_type const& body_;

        public:
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(
                header<isRequest, Fields> const&,
                value_type const& b)
                : body_(b)
            {
            }

            void
            init(error_code& ec)
            {
                ec = {};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                ec = {};
                return {{const_buffers_type{
                    body_.data(), body_.size()}, false}};
            }
        };
    };

    template<
        bool isSplit,
        bool isFinalEmpty
    >
    struct test_body
    {
        struct value_type
        {
            std::string s;
            bool mutable read = false;
        };

        class writer
        {
            int step_ = 0;
            value_type const& body_;

        public:
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(
                header<isRequest, Fields> const&,
                value_type const& b)
                : body_(b)
            {
            }

            void
            init(error_code& ec)
            {
                ec = {};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                ec = {};
                body_.read = true;
                return get(
                    std::integral_constant<bool, isSplit>{},
                    std::integral_constant<bool, isFinalEmpty>{});
            }

        private:
            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::false_type,    // isSplit
                std::false_type)    // isFinalEmpty
            {
                if(body_.s.empty())
                    return boost::none;
                return {{net::buffer(
                    body_.s.data(), body_.s.size()), false}};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::false_type,    // isSplit
                std::true_type)     // isFinalEmpty
            {
                if(body_.s.empty())
                    return boost::none;
                switch(step_)
                {
                case 0:
                    step_ = 1;
                    return {{net::buffer(
                        body_.s.data(), body_.s.size()), true}};
                default:
                    return boost::none;
                }
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::true_type,     // isSplit
                std::false_type)    // isFinalEmpty
            {
                auto const n = (body_.s.size() + 1) / 2;
                switch(step_)
                {
                case 0:
                    if(n == 0)
                        return boost::none;
                    step_ = 1;
                    return {{net::buffer(body_.s.data(), n),
                        body_.s.size() > 1}};
                default:
                    return {{net::buffer(body_.s.data() + n,
                        body_.s.size() - n), false}};
                }
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(
                std::true_type,     // isSplit
                std::true_type)     // isFinalEmpty
            {
                auto const n = (body_.s.size() + 1) / 2;
                switch(step_)
                {
                case 0:
                    if(n == 0)
                        return boost::none;
                    step_ = body_.s.size() > 1 ? 1 : 2;
                    return {{net::buffer(body_.s.data(), n), true}};
                case 1:
                    BOOST_ASSERT(body_.s.size() > 1);
                    step_ = 2;
                    return {{net::buffer(body_.s.data() + n,
                        body_.s.size() - n), true}};
                default:
                    return boost::none;
                }
            }
        };
    };

    struct fail_body
    {
        class writer;

        class value_type
        {
            friend class writer;

            std::string s_;
            test::fail_count& fc_;

        public:
            explicit
            value_type(test::fail_count& fc)
                : fc_(fc)
            {
            }

            value_type&
            operator=(std::string s)
            {
                s_ = std::move(s);
                return *this;
            }
        };

        class writer
        {
            std::size_t n_ = 0;
            value_type const& body_;

        public:
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            explicit
            writer(header<isRequest, Fields> const&, value_type const& b)
                : body_(b)
            {
            }

            void
            init(error_code& ec)
            {
                body_.fc_.fail(ec);
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                if(body_.fc_.fail(ec))
                    return boost::none;
                if(n_ >= body_.s_.size())
                    return boost::none;
                return {{const_buffers_type{
                    body_.s_.data() + n_++, 1}, true}};
            }
        };
    };

    template<bool isRequest, class Body, class Fields>
    static
    std::string
    to_string(message<isRequest, Body, Fields> const& m)
    {
        std::stringstream ss;
        ss << m;
        return ss.str();
    }

    template<bool isRequest>
    bool
    equal_body(string_view sv, string_view body)
    {
        test::stream ts{ioc_, sv}, tr{ioc_};
        ts.connect(tr);
        message<isRequest, string_body, fields> m;
        multi_buffer b;
        ts.close_remote();
        try
        {
            read(ts, b, m);
            return m.body() == body;
        }
        catch(std::exception const& e)
        {
            log << "equal_body: " << e.what() << std::endl;
            return false;
        }
    }

    template<bool isRequest, class Body, class Fields>
    std::string
    str(message<isRequest, Body, Fields> const& m)
    {
        test::stream ts{ioc_}, tr{ioc_};
        ts.connect(tr);
        error_code ec;
        write(ts, m, ec);
        if(ec && ec != error::end_of_stream)
            BOOST_THROW_EXCEPTION(system_error{ec});
        return std::string(tr.str());
    }

    void
    testAsyncWrite(yield_context do_yield)
    {
        {
            response<string_body> m;
            m.version(10);
            m.result(status::ok);
            m.set(field::server, "test");
            m.set(field::content_length, "5");
            m.body() = "*****";
            error_code ec;
            test::stream ts{ioc_}, tr{ioc_};
            ts.connect(tr);
            async_write(ts, m, do_yield[ec]);
            BEAST_EXPECT(! m.keep_alive());
            if(BEAST_EXPECTS(! ec, ec.message()))
                BEAST_EXPECT(tr.str() ==
                    "HTTP/1.0 200 OK\r\n"
                    "Server: test\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****");
        }
        {
            response<string_body> m;
            m.version(11);
            m.result(status::ok);
            m.set(field::server, "test");
            m.set(field::transfer_encoding, "chunked");
            m.body() = "*****";
            error_code ec;
            test::stream ts{ioc_}, tr{ioc_};
            ts.connect(tr);
            async_write(ts, m, do_yield[ec]);
            if(BEAST_EXPECTS(! ec, ec.message()))
                BEAST_EXPECT(tr.str() ==
                    "HTTP/1.1 200 OK\r\n"
                    "Server: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "5\r\n"
                    "*****\r\n"
                    "0\r\n\r\n");
        }
    }

    void
    testFailures(yield_context do_yield)
    {
        static std::size_t constexpr limit = 100;
        std::size_t n;

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc(n);
            test::stream ts{ioc_, fc}, tr{ioc_};
            ts.connect(tr);
            request<fail_body> m(verb::get, "/", 10, fc);
            m.set(field::user_agent, "test");
            m.set(field::connection, "keep-alive");
            m.set(field::content_length, "5");
            m.body() = "*****";
            try
            {
                write(ts, m);
                BEAST_EXPECT(tr.str() ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                pass();
                break;
            }
            catch(std::exception const&)
            {
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc(n);
            test::stream ts{ioc_, fc}, tr{ioc_};
            ts.connect(tr);
            request<fail_body> m{verb::get, "/", 10, fc};
            m.set(field::user_agent, "test");
            m.set(field::transfer_encoding, "chunked");
            m.body() = "*****";
            error_code ec = test::error::test_failure;
            write(ts, m, ec);
            if(! ec)
            {
                BEAST_EXPECT(! m.keep_alive());
                BEAST_EXPECT(tr.str() ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "0\r\n\r\n"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc(n);
            test::stream ts{ioc_, fc}, tr{ioc_};
            ts.connect(tr);
            request<fail_body> m{verb::get, "/", 10, fc};
            m.set(field::user_agent, "test");
            m.set(field::transfer_encoding, "chunked");
            m.body() = "*****";
            error_code ec = test::error::test_failure;
            async_write(ts, m, do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(! m.keep_alive());
                BEAST_EXPECT(tr.str() ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Transfer-Encoding: chunked\r\n"
                    "\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "1\r\n*\r\n"
                    "0\r\n\r\n"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc(n);
            test::stream ts{ioc_, fc}, tr{ioc_};
            ts.connect(tr);
            request<fail_body> m{verb::get, "/", 10, fc};
            m.set(field::user_agent, "test");
            m.set(field::connection, "keep-alive");
            m.set(field::content_length, "5");
            m.body() = "*****";
            error_code ec = test::error::test_failure;
            write(ts, m, ec);
            if(! ec)
            {
                BEAST_EXPECT(tr.str() ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);

        for(n = 0; n < limit; ++n)
        {
            test::fail_count fc(n);
            test::stream ts{ioc_, fc}, tr{ioc_};
            ts.connect(tr);
            request<fail_body> m{verb::get, "/", 10, fc};
            m.set(field::user_agent, "test");
            m.set(field::connection, "keep-alive");
            m.set(field::content_length, "5");
            m.body() = "*****";
            error_code ec = test::error::test_failure;
            async_write(ts, m, do_yield[ec]);
            if(! ec)
            {
                BEAST_EXPECT(tr.str() ==
                    "GET / HTTP/1.0\r\n"
                    "User-Agent: test\r\n"
                    "Connection: keep-alive\r\n"
                    "Content-Length: 5\r\n"
                    "\r\n"
                    "*****"
                );
                break;
            }
        }
        BEAST_EXPECT(n < limit);
    }

    void
    testOutput()
    {
        // auto content-length HTTP/1.0
        {
            request<string_body> m;
            m.method(verb::get);
            m.target("/");
            m.version(10);
            m.set(field::user_agent, "test");
            m.body() = "*";
            m.prepare_payload();
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.0\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            );
        }
        // no content-length HTTP/1.0
        {
            request<unsized_body> m;
            m.method(verb::get);
            m.target("/");
            m.version(10);
            m.set(field::user_agent, "test");
            m.body() = "*";
            m.prepare_payload();
            test::stream ts{ioc_}, tr{ioc_};
            ts.connect(tr);
            error_code ec;
            write(ts, m, ec);
            BEAST_EXPECT(! m.keep_alive());
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(tr.str() ==
                "GET / HTTP/1.0\r\n"
                "User-Agent: test\r\n"
                "\r\n"
                "*"
            );
        }
        // auto content-length HTTP/1.1
        {
            request<string_body> m;
            m.method(verb::get);
            m.target("/");
            m.version(11);
            m.set(field::user_agent, "test");
            m.body() = "*";
            m.prepare_payload();
            BEAST_EXPECT(str(m) ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 1\r\n"
                "\r\n"
                "*"
            );
        }
        // no content-length HTTP/1.1
        {
            request<unsized_body> m;
            m.method(verb::get);
            m.target("/");
            m.version(11);
            m.set(field::user_agent, "test");
            m.body() = "*";
            m.prepare_payload();
            test::stream ts{ioc_}, tr{ioc_};
            ts.connect(tr);
            error_code ec;
            write(ts, m, ec);
            BEAST_EXPECT(tr.str() ==
                "GET / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "1\r\n"
                "*\r\n"
                "0\r\n\r\n"
            );
        }
    }

    void test_std_ostream()
    {
        // Conversion to std::string via operator<<
        {
            request<string_body> m;
            m.method(verb::get);
            m.target("/");
            m.version(11);
            m.set(field::user_agent, "test");
            m.body() = "*";
            BEAST_EXPECT(to_string(m) ==
                "GET / HTTP/1.1\r\nUser-Agent: test\r\n\r\n*");
        }

        // Output to std::ostream
        {
            request<string_body> m{verb::get, "/", 11};
            std::stringstream ss;
            ss << m;
            BEAST_EXPECT(ss.str() ==
                "GET / HTTP/1.1\r\n"
                "\r\n");
        }

        // Output header to std::ostream
        {
            request<string_body> m{verb::get, "/", 11};
            std::stringstream ss;
            ss << m.base();
            BEAST_EXPECT(ss.str() ==
                "GET / HTTP/1.1\r\n"
                "\r\n");
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
            test::stream ts{ioc};
            BEAST_EXPECT(handler::count() == 0);
            request<string_body> m;
            m.method(verb::get);
            m.version(11);
            m.target("/");
            m.set("Content-Length", 5);
            m.body() = "*****";
            async_write(ts, m, handler{});
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
                test::stream ts{ioc}, tr{ioc};
                ts.connect(tr);
                BEAST_EXPECT(handler::count() == 0);
                request<string_body> m;
                m.method(verb::get);
                m.version(11);
                m.target("/");
                m.set("Content-Length", 5);
                m.body() = "*****";
                async_write(ts, m, handler{});
                BEAST_EXPECT(handler::count() > 0);
            }
            BEAST_EXPECT(handler::count() == 0);
        }
    }

    template<
        class Stream,
        bool isRequest, class Body, class Fields>
    void
    do_write(
        Stream& stream,
        message<isRequest, Body, Fields> const& m,
        error_code& ec)
    {
        serializer<isRequest, Body, Fields> sr{m};
        for(;;)
        {
            write_some(stream, sr, ec);
            if(ec)
                return;
            if(sr.is_done())
                break;
        }
    }

    template<
        class Stream,
        bool isRequest, class Body, class Fields>
    void
    do_async_write(
        Stream& stream,
        message<isRequest, Body, Fields> const& m,
        error_code& ec,
        yield_context yield)
    {
        serializer<isRequest, Body, Fields> sr{m};
        for(;;)
        {
            async_write_some(stream, sr, yield[ec]);
            if(ec)
                return;
            if(sr.is_done())
                break;
        }
    }

    template<class Body>
    void
    testWriteStream(net::yield_context yield)
    {
        test::stream ts{ioc_}, tr{ioc_};
        ts.connect(tr);
        ts.write_size(3);

        response<Body> m0;
        m0.version(11);
        m0.result(status::ok);
        m0.reason("OK");
        m0.set(field::server, "test");
        m0.body().s = "Hello, world!\n";

        {
            std::string const result =
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "\r\n"
                "Hello, world!\n";
            {
                auto m = m0;
                error_code ec;
                do_write(ts, m, ec);
                BEAST_EXPECT(tr.str() == result);
                BEAST_EXPECT(equal_body<false>(
                    tr.str(), m.body().s));
                tr.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_async_write(ts, m, ec, yield);
                BEAST_EXPECT(tr.str() == result);
                BEAST_EXPECT(equal_body<false>(
                    tr.str(), m.body().s));
                tr.clear();
            }
            {
                auto m = m0;
                response_serializer<Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    write_some(ts, sr);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body().read);
                tr.clear();
            }
            {
                auto m = m0;
                response_serializer<Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    async_write_some(ts, sr, yield);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body().read);
                tr.clear();
            }
        }
        {
            m0.set("Transfer-Encoding", "chunked");
            {
                auto m = m0;
                error_code ec;
                do_write(ts, m, ec);
                BEAST_EXPECT(equal_body<false>(
                    tr.str(), m.body().s));
                tr.clear();
            }
            {
                auto m = m0;
                error_code ec;
                do_async_write(ts, m, ec, yield);
                BEAST_EXPECT(equal_body<false>(
                    tr.str(), m.body().s));
                tr.clear();
            }
            {
                auto m = m0;
                response_serializer<Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    write_some(ts, sr);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body().read);
                tr.clear();
            }
            {
                auto m = m0;
                response_serializer<Body, fields> sr{m};
                sr.split(true);
                for(;;)
                {
                    async_write_some(ts, sr, yield);
                    if(sr.is_header_done())
                        break;
                }
                BEAST_EXPECT(! m.body().read);
                tr.clear();
            }
        }
    }

    void
    testIssue655()
    {
        net::io_context ioc;
        test::stream ts{ioc}, tr{ioc};
        ts.connect(tr);
        response<empty_body> res;
        res.chunked(true);
        response_serializer<empty_body> sr{res};
        async_write_header(ts, sr,
            [&](error_code const&, std::size_t)
            {
            });
        ioc.run();
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
            request<empty_body> m;
            request_serializer<empty_body, fields> sr{m};
            async_write_some(ts, sr,
                net::bind_executor(
                    s, copyable_handler{}));
        }
        {
            net::io_context ioc;
            strand s{ioc.get_executor()};
            test::stream ts{ioc};
            flat_buffer b;
            request<empty_body> m;
            request_serializer<empty_body, fields> sr{m};
            async_write(ts, sr,
                net::bind_executor(
                    s, copyable_handler{}));
        }
        {
            net::io_context ioc;
            strand s{ioc.get_executor()};
            test::stream ts{ioc};
            flat_buffer b;
            request<empty_body> m;
            async_write(ts, m,
                net::bind_executor(
                    s, copyable_handler{}));
        }
    }

    struct const_body_writer
    {
        struct value_type{};

        struct writer
        {
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(
                header<isRequest, Fields> const&,
                value_type const&)
            {
            }

            void
            init(error_code& ec)
            {
                ec = {};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                ec = {};
                return {{const_buffers_type{"", 0}, false}};
            }
        };
    };

    struct mutable_body_writer
    {
        struct value_type{};

        struct writer
        {
            using const_buffers_type =
                net::const_buffer;

            template<bool isRequest, class Fields>
            writer(
                header<isRequest, Fields>&,
                value_type&)
            {
            }

            void
            init(error_code& ec)
            {
                ec = {};
            }

            boost::optional<std::pair<const_buffers_type, bool>>
            get(error_code& ec)
            {
                ec = {};
                return {{const_buffers_type{"", 0}, false}};
            }
        };
    };

    void
    testBodyWriters()
    {
        {
            test::stream s{ioc_};
            message<true, const_body_writer> m;
            try
            {
                write(s, m);
            }
            catch(std::exception const&)
            {
            }
        }
        {
            error_code ec;
            test::stream s{ioc_};
            message<true, const_body_writer> m;
            write(s, m, ec);
        }
        {
            test::stream s{ioc_};
            message<true, mutable_body_writer> m;
            try
            {
                write(s, m);
            }
            catch(std::exception const&)
            {
            }
        }
        {
            error_code ec;
            test::stream s{ioc_};
            message<true, mutable_body_writer> m;
            write(s, m, ec);
        }
    }

    void
    run() override
    {
        testIssue655();
        yield_to(
            [&](yield_context yield)
            {
                testAsyncWrite(yield);
                testFailures(yield);
            });
        testOutput();
        test_std_ostream();
        testIoService();
        yield_to(
            [&](yield_context yield)
            {
                testWriteStream<test_body<false, false>>(yield);
                testWriteStream<test_body<false,  true>>(yield);
                testWriteStream<test_body< true, false>>(yield);
                testWriteStream<test_body< true,  true>>(yield);
            });
        testAsioHandlerInvoke();
        testBodyWriters();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,write);

} // http
} // beast
} // boost
