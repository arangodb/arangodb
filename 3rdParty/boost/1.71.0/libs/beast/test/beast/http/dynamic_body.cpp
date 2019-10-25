//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/dynamic_body.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class dynamic_body_test : public beast::unit_test::suite
{
    net::io_context ioc_;

public:
    template<bool isRequest, class Body, class Fields>
    static
    std::string
    to_string(message<isRequest, Body, Fields> const& m)
    {
        std::stringstream ss;
        ss << m;
        return ss.str();
    }

    void
    test_success()
    {
        std::string const s =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 3\r\n"
            "\r\n"
            "xyz";
        test::stream ts(ioc_, s);
        response_parser<dynamic_body> p;
        multi_buffer b;
        read(ts, b, p);
        auto const& m = p.get();
        BEAST_EXPECT(buffers_to_string(m.body().data()) == "xyz");
        BEAST_EXPECT(to_string(m) == s);
    }

    void
    test_issue1581()
    {
        std::string const s =
            "HTTP/1.1 200 OK\r\n"
            "Server: test\r\n"
            "Content-Length: 132\r\n"
            "\r\n"
            "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyz"
            "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyz"
            "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyz"
            "xyzxyzxyzxyzxyzxyzxyzxyzxyzxyzxyz";
        test::stream ts(ioc_, s);
        response_parser<dynamic_body> p;
        multi_buffer b;
        p.get().body().max_size(64);
        error_code ec;
        read(ts, b, p, ec);
        BEAST_EXPECT(ec == http::error::buffer_overflow);
    }

    void
    run() override
    {
        test_success();
        test_issue1581();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,dynamic_body);

} // http
} // beast
} // boost
