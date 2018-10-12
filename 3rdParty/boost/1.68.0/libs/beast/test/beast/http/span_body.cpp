//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/span_body.hpp>

#include <boost/beast/http/message.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

struct span_body_test
    : public beast::unit_test::suite
{
    void
    testSpanBody()
    {
        {
            using B = span_body<char const>;
            request<B> req;

            BEAST_EXPECT(req.body().size() == 0);
            BEAST_EXPECT(B::size(req.body()) == 0);

            req.body() = B::value_type("xyz", 3);
            BEAST_EXPECT(req.body().size() == 3);
            BEAST_EXPECT(B::size(req.body()) == 3);

            B::writer r{req, req.body()};
            error_code ec;
            r.init(ec);
            BEAST_EXPECTS(! ec, ec.message());
            auto const buf = r.get(ec);
            BEAST_EXPECTS(! ec, ec.message());
            if(! BEAST_EXPECT(buf != boost::none))
                return;
            BEAST_EXPECT(boost::asio::buffer_size(buf->first) == 3);
            BEAST_EXPECT(! buf->second);
        }
        {
            char buf[5];
            using B = span_body<char>;
            request<B> req;
            req.body() = span<char>{buf, sizeof(buf)};
            B::reader w{req, req.body()};
            error_code ec;
            w.init(boost::none, ec);
            BEAST_EXPECTS(! ec, ec.message());
            w.put(boost::asio::const_buffer{
                "123", 3}, ec);
            BEAST_EXPECTS(! ec, ec.message());
            BEAST_EXPECT(buf[0] == '1');
            BEAST_EXPECT(buf[1] == '2');
            BEAST_EXPECT(buf[2] == '3');
            w.put(boost::asio::const_buffer{
                "456", 3}, ec);
            BEAST_EXPECTS(ec == error::buffer_overflow, ec.message());
        }
    }

    void
    run() override
    {
        testSpanBody();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,span_body);

} // http
} // beast
} // boost
