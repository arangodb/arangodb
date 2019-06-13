//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/experimental/core/flat_stream.hpp>

#include <boost/beast/test/websocket.hpp>
#include <boost/beast/test/yield_to.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <initializer_list>
#include <vector>

namespace boost {
namespace beast {

class flat_stream_test
    : public unit_test::suite
    , public test::enable_yield_to
{
public:
    void
    testSplit()
    {
        auto const check =
            [&](
                std::initializer_list<int> v0,
                std::size_t limit,
                unsigned long count,
                bool copy)
            {
                std::vector<boost::asio::const_buffer> v;
                v.reserve(v0.size());
                for(auto const n : v0)
                    v.emplace_back("", n);
                auto const result =
                    boost::beast::detail::flat_stream_base::coalesce(v, limit);
                BEAST_EXPECT(result.first == count);
                BEAST_EXPECT(result.second == copy);
                return result;
            };
        check({},           1,    0, false);
        check({1,2},        1,    1, false);
        check({1,2},        2,    1, false);
        check({1,2},        3,    3, true);
        check({1,2},        4,    3, true);
        check({1,2,3},      1,    1, false);
        check({1,2,3},      2,    1, false);
        check({1,2,3},      3,    3, true);
        check({1,2,3},      4,    3, true);
        check({1,2,3},      7,    6, true);
        check({1,2,3,4},    3,    3, true);
    }

    void
    testHttp()
    {
        pass();
    }

    void
    testWebsocket()
    {
        {
            error_code ec;
            test::ws_echo_server es{log};
            boost::asio::io_context ioc;
            websocket::stream<flat_stream<test::stream>> ws{ioc};
            ws.next_layer().next_layer().connect(es.stream());
            ws.handshake("localhost", "/", ec);
            BEAST_EXPECTS(! ec, ec.message());
            ws.close({}, ec);
            BEAST_EXPECTS(! ec, ec.message());
        }
        {
            test::ws_echo_server es{log};
            boost::asio::io_context ioc;
            websocket::stream<flat_stream<test::stream>> ws{ioc};
            ws.next_layer().next_layer().connect(es.stream());
            ws.async_handshake("localhost", "/",
                [&](error_code)
                {
                    ws.async_close({},
                        [&](error_code)
                        {
                        });
                });
            ioc.run();
        }
    }

    void
    run() override
    {
        testSplit();
        testHttp();
        testWebsocket();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,flat_stream);

} // beast
} // boost
