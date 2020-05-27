//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/http/icy_stream.hpp>

#include <boost/beast/core/buffers_adaptor.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/read_size.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <array>
#include <memory>
#include <string>

namespace boost {
namespace beast {
namespace http {

class icy_stream_test
    : public unit_test::suite
{
public:
    void
    doMatrix(string_view in, string_view out)
    {
        using net::mutable_buffer;
        net::io_context ioc;
        auto len = out.size() + 8;
        std::unique_ptr<char[]> p(new char[len]);
        for(std::size_t i = 1; i < len; ++i)
        {
            std::array<mutable_buffer, 2> mbs{{
                mutable_buffer(p.get(), i),
                mutable_buffer(p.get() + i, len - i)}};
            for(std::size_t j = 1; j < in.size(); ++j)
            {
                for(std::size_t k = 1; k < len; ++k)
                {
                    // sync
                    {
                        buffers_adaptor<decltype(mbs)> ba(mbs);
                        std::memset(p.get(), 0, len);

                        icy_stream<test::stream> is{ioc};
                        is.next_layer().read_size(j);
                        is.next_layer().append(in);
                        connect(is.next_layer()).close();

                        error_code ec;
                        for(;;)
                        {
                            ba.commit(is.read_some(
                                ba.prepare(read_size(ba, k)), ec));
                            if(ec)
                                break;
                        }
                        if(! BEAST_EXPECTS(
                            ec == net::error::eof, ec.message()))
                            continue;
                        auto const s = buffers_to_string(ba.data());
                        BEAST_EXPECTS(s == out, s);
                    }
                    // async
                    {
                        buffers_adaptor<decltype(mbs)> ba(mbs);
                        std::memset(p.get(), 0, len);

                        icy_stream<test::stream> is{ioc};
                        is.next_layer().read_size(j);
                        is.next_layer().append(in);
                        connect(is.next_layer()).close();

                        error_code ec;
                        for(;;)
                        {
                            is.async_read_some(
                                ba.prepare(read_size(ba, k)),
                                [&ec, &ba](error_code ec_, std::size_t n)
                                {
                                    ec = ec_;
                                    ba.commit(n);
                                });
                            ioc.run();
                            ioc.restart();
                            if(ec)
                                break;
                        }
                        if(! BEAST_EXPECTS(
                            ec == net::error::eof, ec.message()))
                            continue;
                        auto const s = buffers_to_string(ba.data());
                        if(! BEAST_EXPECTS(s == out, s))
                        {
                            s.size();
                        }
                    }
                }
            }
        }
    }

    void
    testStream()
    {
        doMatrix("HTTP/1.1 200 OK\r\n", "HTTP/1.1 200 OK\r\n");
        doMatrix("ICY 200 OK\r\n",      "HTTP/1.1 200 OK\r\n");
    }

    void
    run() override
    {
        testStream();
    }
};

BEAST_DEFINE_TESTSUITE(beast,http,icy_stream);

} // http
} // beast
} // boost
