//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// VFALCO This causes a compilation error
#include <boost/beast/websocket/stream.hpp>

// Test that header file is self-contained.
#include <boost/beast/websocket/ssl.hpp>

#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

namespace boost {
namespace beast {
namespace websocket {

class ssl_test : public unit_test::suite
{
public:
    template<class Socket>
    void
    testTeardown()
    {
        net::io_context ioc;
        net::ssl::context ctx(net::ssl::context::tlsv12);
        Socket ss(ioc, ctx);

        struct handler
        {
            void
            operator()(error_code)
            {
            }
        };

        websocket::stream<Socket> ws(ioc, ctx);

        BEAST_EXPECT(static_cast<
            void(websocket::stream<Socket>::*)(
                close_reason const&)>(
            &websocket::stream<Socket>::close));

        BEAST_EXPECT(static_cast<
            void(websocket::stream<Socket>::*)(
                close_reason const&, error_code&)>(
            &websocket::stream<Socket>::close));
    }

    void
    run() override
    {
        testTeardown<
            boost::asio::ssl::stream<
                boost::asio::basic_stream_socket<
                    boost::asio::ip::tcp,
                    net::any_io_executor>>>();

    }
};

BEAST_DEFINE_TESTSUITE(beast,websocket,ssl);

} // websocket
} // beast
} // boost
