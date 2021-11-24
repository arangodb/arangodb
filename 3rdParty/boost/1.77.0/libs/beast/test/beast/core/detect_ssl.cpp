//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detect_ssl.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/beast/_experimental/test/handler.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/string.hpp>
#include <boost/core/exchange.hpp>
#if BOOST_ASIO_HAS_CO_AWAIT
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#endif
namespace boost {
namespace beast {

class detect_ssl_test : public unit_test::suite
{
public:
    void
    testDetect()
    {
        auto const yes =
            [](int n, string_view s)
            {
                BEAST_EXPECT(detail::is_tls_client_hello(
                    net::const_buffer(s.data(), n)));
            };

        auto const no =
            [](int n, string_view s)
            {
                BEAST_EXPECT(! detail::is_tls_client_hello(
                    net::const_buffer(s.data(), n)));
            };

        auto const maybe =
            [](int n, string_view s)
            {
                BEAST_EXPECT(boost::indeterminate(
                    detail::is_tls_client_hello(
                        net::const_buffer(s.data(), n))));
            };

        maybe(  0, "\x00\x00\x00\x00\x00\x00\x00\x00\x00");
        no   (  1, "\x01\x00\x00\x00\x00\x00\x00\x00\x00");
        maybe(  1, "\x16\x00\x00\x00\x00\x00\x00\x00\x00");
        maybe(  4, "\x16\x00\x00\x00\x00\x00\x00\x00\x00");
        no   (  5, "\x16\x00\x00\x00\x00\x00\x00\x00\x00");
        maybe(  5, "\x16\x00\x00\x01\x00\x00\x00\x00\x00");
        no   (  8, "\x16\x00\x00\x01\x00\x00\x00\x00\x00");
        maybe(  8, "\x16\x00\x00\x01\x00\x01\x00\x00\x00");
        no   (  9, "\x16\x00\x00\x01\x00\x01\x01\x00\x00");
        yes  (  9, "\x16\x00\x00\x01\x00\x01\x00\x00\x00");
    }

    void
    testRead()
    {
        net::io_context ioc;

        // true

        {
            error_code ec;
            flat_buffer b;
            test::stream s1(ioc);
            s1.append({"\x16\x00\x00\x01\x00\x01\x00\x00\x00", 9});
            auto result = detect_ssl(s1, b, ec);
            BEAST_EXPECT(result == true);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // true

        {
            error_code ec;
            flat_buffer b;
            test::stream s1(ioc);
            auto s2 = test::connect(s1);
            s1.append({"\x16\x00\x00\x01\x00\x01\x00\x00\x00", 9});
            s2.close();
            auto result = detect_ssl(s1, b, ec);
            BEAST_EXPECT(result == true);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // false

        {
            error_code ec;
            flat_buffer b;
            test::stream s1(ioc);
            s1.append({"\x16\x00\x00\x01\x00\x01\x01\x00\x00", 9});
            auto result = detect_ssl(s1, b, ec);
            BEAST_EXPECT(result == false);
            BEAST_EXPECTS(! ec, ec.message());
        }

        // eof
        {
            error_code ec;
            flat_buffer b;
            test::stream s1(ioc);
            auto s2 = test::connect(s1);
            s1.append({"\x16\x00\x00\x01\x00", 5});
            s2.close();
            auto result = detect_ssl(s1, b, ec);
            BEAST_EXPECT(result == false);
            BEAST_EXPECT(ec);
        }
    }

    void
    testAsyncRead()
    {
        net::io_context ioc;

        // true

        {
            flat_buffer b;
            test::stream s1(ioc);
            s1.append({"\x16\x00\x00\x01\x00\x01\x00\x00\x00", 9});
            async_detect_ssl(s1, b, test::success_handler());
            test::run(ioc);
        }

        // true

        {
            flat_buffer b;
            test::stream s1(ioc);
            auto s2 = test::connect(s1);
            s1.append({"\x16\x00\x00\x01\x00\x01\x00\x00\x00", 9});
            s2.close();
            async_detect_ssl(s1, b, test::success_handler());
            test::run(ioc);
        }

        // false

        {
            flat_buffer b;
            test::stream s1(ioc);
            s1.append({"\x16\x00\x00\x01\x00\x01\x01\x00\x00", 9});
            async_detect_ssl(s1, b, test::success_handler());
            test::run(ioc);
        }

        // eof
        {
            flat_buffer b;
            test::stream s1(ioc);
            auto s2 = test::connect(s1);
            s1.append({"\x16\x00\x00\x01\x00", 5});
            s2.close();
            async_detect_ssl(s1, b,
                test::fail_handler(net::error::eof));
            test::run(ioc);
        }
    }

#if BOOST_ASIO_HAS_CO_AWAIT
    void testAwaitableCompiles(test::stream& stream, flat_buffer& b)
    {
        static_assert(
            std::is_same_v<
                net::awaitable<bool>, decltype(
                async_detect_ssl(stream, b, net::use_awaitable))>);
    }
#endif

    void
    run() override
    {
        testDetect();
        testRead();
        testAsyncRead();
#if BOOST_ASIO_HAS_CO_AWAIT
        boost::ignore_unused(&detect_ssl_test::testAwaitableCompiles);
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,detect_ssl);

} // beast
} // boost
