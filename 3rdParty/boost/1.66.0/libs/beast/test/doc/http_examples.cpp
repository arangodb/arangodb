//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#include "example/doc/http_examples.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/core/detail/clamp.hpp>
#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/http/chunk_encode.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/test/stream.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <sstream>
#include <array>
#include <limits>
#include <list>
#include <sstream>
#include <vector>

namespace boost {
namespace beast {
namespace http {

class examples_test
    : public beast::unit_test::suite
    , public beast::test::enable_yield_to
{
public:
    // two threads, for some examples
    examples_test()
        : enable_yield_to(2)
    {
    }

    template<bool isRequest, class Body, class Fields>
    static
    std::string
    to_string(message<isRequest, Body, Fields> const& m)
    {
        std::stringstream ss;
        ss << m;
        return ss.str();
    }

    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        std::string s;
        s.reserve(buffer_size(bs));
        for(auto b : beast::detail::buffers_range(bs))
            s.append(reinterpret_cast<
                char const*>(b.data()), b.size());
        return s;
    }

    template<bool isRequest>
    bool
    equal_body(string_view sv, string_view body)
    {
        test::stream ts{ioc_, sv};
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

    void
    doExpect100Continue()
    {
        test::stream ts{ioc_}, tr{ioc_};
        ts.connect(tr);
        yield_to(
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                receive_expect_100_continue(
                    tr, buffer, ec);
                BEAST_EXPECTS(! ec, ec.message());
            },
            [&](yield_context)
            {
                flat_buffer buffer;
                request<string_body> req;
                req.version(11);
                req.method_string("POST");
                req.target("/");
                req.insert(field::user_agent, "test");
                req.body() = "Hello, world!";
                req.prepare_payload();

                error_code ec;
                send_expect_100_continue(
                    ts, buffer, req, ec);
                BEAST_EXPECTS(! ec, ec.message());
            });
    }

    void
    doCgiResponse()
    {
        std::string const s = "Hello, world!";
        test::stream t0{ioc_, s};
        t0.read_size(3);
        t0.close_remote();
        test::stream t1{ioc_}, t1r{ioc_};
        t1.connect(t1r);
        error_code ec;
        send_cgi_response(t0, t1, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<false>(t1r.str(), s));
    }

    void
    doRelay()
    {
        request<string_body> req;
        req.version(11);
        req.method_string("POST");
        req.target("/");
        req.insert(field::user_agent, "test");
        req.body() = "Hello, world!";
        req.prepare_payload();

        test::stream ds{ioc_}, dsr{ioc_};
        ds.connect(dsr);
        dsr.read_size(3);
        test::stream us{ioc_}, usr{ioc_};
        us.connect(usr);
        us.write_size(3);

        error_code ec;
        write(ds, req);
        BEAST_EXPECTS(! ec, ec.message());
        ds.close();

        flat_buffer buffer;
        relay<true>(us, dsr, buffer, ec,
            [&](header<true, fields>& h, error_code& ev)
            {
                ev = {};
                h.erase("Content-Length");
                h.set("Transfer-Encoding", "chunked");
            });
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(equal_body<true>(
            usr.str(), req.body()));
    }

    void
    doReadStdStream()
    {
        std::string const s =
            "HTTP/1.0 200 OK\r\n"
            "User-Agent: test\r\n"
            "\r\n"
            "Hello, world!";
        std::istringstream is(s);
        error_code ec;
        flat_buffer buffer;
        response<string_body> res;
        read_istream(is, buffer, res, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(to_string(res) == s);
    }

    void
    doWriteStdStream()
    {
        std::ostringstream os;
        request<string_body> req;
        req.version(11);
        req.method(verb::get);
        req.target("/");
        req.insert(field::user_agent, "test");
        error_code ec;
        write_ostream(os, req, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(to_string(req) == os.str());
    }

    void
    doCustomParser()
    {
        {
            string_view s{
                "POST / HTTP/1.1\r\n"
                "User-Agent: test\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Hello, world!"
            };
            error_code ec;
            custom_parser<true> p;
            p.put(boost::asio::buffer(
                s.data(), s.size()), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
        {
            string_view s{
                "HTTP/1.1 200 OK\r\n"
                "Server: test\r\n"
                "Transfer-Encoding: chunked\r\n"
                "\r\n"
                "d\r\n"
                "Hello, world!"
                "\r\n"
                "0\r\n\r\n"
            };
            error_code ec;
            custom_parser<false> p;
            p.put(boost::asio::buffer(
                s.data(), s.size()), ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
    }

    void
    doHEAD()
    {
        test::stream ts{ioc_}, tr{ioc_};
        ts.connect(tr);
        yield_to(
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                do_server_head(tr, buffer, ec);
                BEAST_EXPECTS(! ec, ec.message());
            },
            [&](yield_context)
            {
                error_code ec;
                flat_buffer buffer;
                auto res = do_head_request(ts, buffer, "/", ec);
                BEAST_EXPECTS(! ec, ec.message());
            });
    }

    struct handler
    {
        std::string body;

        template<class Body>
        void
        operator()(request<Body>&&)
        {
        }

        void
        operator()(request<string_body>&& req)
        {
            body = req.body();
        }
    };

    void
    doDeferredBody()
    {
        test::stream ts(ioc_,
            "POST / HTTP/1.1\r\n"
            "User-Agent: test\r\n"
            "Content-Type: multipart/form-data\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "Hello, world!");

        handler h;
        flat_buffer buffer;
        do_form_request(ts, buffer, h);
        BEAST_EXPECT(h.body == "Hello, world!");
    }

    //--------------------------------------------------------------------------

    void
    doIncrementalRead()
    {
        test::stream ts{ioc_};
        std::string s(2048, '*');
        ostream(ts.buffer()) <<
            "HTTP/1.1 200 OK\r\n"
            "Content-Length: 2048\r\n"
            "Server: test\r\n"
            "\r\n" <<
            s;
        error_code ec;
        flat_buffer b;
        std::stringstream ss;
        read_and_print_body<false>(ss, ts, b, ec);
        if(BEAST_EXPECTS(! ec, ec.message()))
            BEAST_EXPECT(ss.str() == s);
    }

    //--------------------------------------------------------------------------

    void
    doExplicitChunkSerialize()
    {
        auto const buf =
            [](string_view s)
            {
                return boost::asio::const_buffer{
                    s.data(), s.size()};
            };
        test::stream ts{ioc_}, tr{ioc_};
        ts.connect(tr);
        
        response<empty_body> res{status::ok, 11};
        res.set(field::server, "test");
        res.set(field::accept, "Expires, Content-MD5");
        res.chunked(true);

        error_code ec;
        response_serializer<empty_body> sr{res};
        write_header(ts, sr, ec);

        chunk_extensions exts;

        boost::asio::write(ts,
            make_chunk(buf("First")), ec);

        exts.insert("quality", "1.0");
        boost::asio::write(ts,
            make_chunk(buf("Hello, world!"), exts), ec);

        exts.clear();
        exts.insert("file", "abc.txt");
        exts.insert("quality", "0.7");
        boost::asio::write(ts,
            make_chunk(buf("The Next Chunk"), std::move(exts)), ec);

        exts.clear();
        exts.insert("last");
        boost::asio::write(ts,
            make_chunk(buf("Last one"), std::move(exts),
                std::allocator<double>{}), ec);

        fields trailers;
        trailers.set(field::expires, "never");
        trailers.set(field::content_md5, "f4a5c16584f03d90");

        boost::asio::write(ts,
            make_chunk_last(
                trailers,
                std::allocator<double>{}
                    ), ec);
        BEAST_EXPECT(
            to_string(tr.buffer().data()) ==
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Accept: Expires, Content-MD5\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "First\r\n"
            "d;quality=1.0\r\n"
            "Hello, world!\r\n"
            "e;file=abc.txt;quality=0.7\r\n"
            "The Next Chunk\r\n"
            "8;last\r\n"
            "Last one\r\n"
            "0\r\n"
            "Expires: never\r\n"
            "Content-MD5: f4a5c16584f03d90\r\n"
            "\r\n");
    }

    //--------------------------------------------------------------------------

    void
    doExplicitChunkParse()
    {
        test::stream ts(ioc_,
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Trailer: Expires, Content-MD5\r\n"
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "5\r\n"
            "First\r\n"
            "d;quality=1.0\r\n"
            "Hello, world!\r\n"
            "e;file=abc.txt;quality=0.7\r\n"
            "The Next Chunk\r\n"
            "8;last\r\n"
            "Last one\r\n"
            "0\r\n"
            "Expires: never\r\n"
            "Content-MD5: f4a5c16584f03d90\r\n"
            "\r\n");


        error_code ec;
        flat_buffer b;
        std::stringstream ss;
        print_chunked_body<false>(ss, ts, b, ec);
        BEAST_EXPECTS(! ec, ec.message());
        BEAST_EXPECT(ss.str() ==
            "Chunk Body: First\n"
            "Extension: quality = 1.0\n"
            "Chunk Body: Hello, world!\n"
            "Extension: file = abc.txt\n"
            "Extension: quality = 0.7\n"
            "Chunk Body: The Next Chunk\n"
            "Extension: last\n"
            "Chunk Body: Last one\n"
            "Expires: never\n"
            "Content-MD5: f4a5c16584f03d90\n");
    }

    //--------------------------------------------------------------------------

    void
    run()
    {
        doExpect100Continue();
        doCgiResponse();
        doRelay();
        doReadStdStream();
        doWriteStdStream();
        doCustomParser();
        doHEAD();
        doDeferredBody();
        doIncrementalRead();
        doExplicitChunkSerialize();
        doExplicitChunkParse();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,examples);

} // http
} // beast
} // boost
