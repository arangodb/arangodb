//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/http/dynamic_body.hpp>

#include <boost/beast/core/ostream.hpp>
#include <boost/beast/http/fields.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/test/stream.hpp>
#include <boost/beast/unit_test/suite.hpp>

namespace boost {
namespace beast {
namespace http {

class dynamic_body_test : public beast::unit_test::suite
{
    boost::asio::io_context ioc_;

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

    template<class ConstBufferSequence>
    static
    std::string
    to_string(ConstBufferSequence const& bs)
    {
        std::string s;
        s.reserve(boost::asio::buffer_size(bs));
        for(auto b : beast::detail::buffers_range(bs))
            s.append(reinterpret_cast<char const*>(b.data()),
                b.size());
        return s;
    }

    void
    run() override
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
        BEAST_EXPECT(to_string(m.body().data()) == "xyz");
        BEAST_EXPECT(to_string(m) == s);
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,dynamic_body);

} // http
} // beast
} // boost
