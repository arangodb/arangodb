//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/_experimental/test/stream.hpp>

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/handler.hpp>

#include <boost/asio/ssl/stream.hpp>
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
#include <boost/asio/use_awaitable.hpp>
#endif

#define DEF boost::asio::use_future_t
namespace boost {
namespace beast {

class stream_test
    : public unit_test::suite
{
public:
    void
    testTestStream()
    {
        char buf[1] = {};
        net::mutable_buffer m0;
        net::mutable_buffer m1(buf, sizeof(buf));

        {
            net::io_context ioc;

            {
                test::stream ts(ioc);
            }
            {
                test::stream ts(ioc);
                ts.close();
            }
            {
                test::stream t1(ioc);
                auto t2 = connect(t1);
            }
            {
                test::stream t1(ioc);
                auto t2 = connect(t1);
                t2.close();
            }
        }
        {
            // abandon
            net::io_context ioc;
            test::stream ts(ioc);
            ts.async_read_some(m1,
                [](error_code, std::size_t)
                {
                    BEAST_FAIL();
                });
        }
        //---
        {
            net::io_context ioc;
            {
                test::stream ts(ioc);
                ts.async_read_some(m1,
                    test::fail_handler(
                        net::error::operation_aborted));
            }
            test::run(ioc);
        }
        {
            net::io_context ioc;
            test::stream ts(ioc);
            ts.async_read_some(m1,
                test::fail_handler(
                    net::error::operation_aborted));
            ts.close(); 
            test::run(ioc);
        }
        {
            net::io_context ioc;
            test::stream t1(ioc);
            auto t2 = connect(t1);
            t1.async_read_some(m1,
                test::fail_handler(
                    net::error::eof));
            t2.close();
            test::run(ioc);
        }
        {
            net::io_context ioc;
            test::stream t1(ioc);
            auto t2 = connect(t1);
            t1.async_read_some(m1,
                test::fail_handler(
                    net::error::operation_aborted));
            t1.close();
            test::run(ioc);
        }
    }

    void
    testSharedAbandon()
    {
        struct handler
        {
            std::shared_ptr<test::stream> ts_;

            void
            operator()(error_code, std::size_t)
            {
            }
        };

        char buf[1] = {};
        net::mutable_buffer m1(buf, sizeof(buf));

        std::weak_ptr<test::stream> wp;

        {
            net::io_context ioc;
            {
                auto sp = std::make_shared<test::stream>(ioc);

                sp->async_read_some(m1, handler{sp});
                wp = sp;
            }
        }
        BEAST_EXPECT(! wp.lock());
    }

    void
    testLifetimeViolation()
    {
        // This should assert
        std::shared_ptr<test::stream> sp;
        {
            net::io_context ioc;
            sp = std::make_shared<test::stream>(ioc);
        }
        sp.reset();
    }

    void
    testAsioSSLCompat()
    {
        BOOST_STATIC_ASSERT(
            std::is_same<
                boost::asio::ssl::stream<test::stream>::
                    lowest_layer_type,
                test::stream>::value);
    }

#if defined(BOOST_ASIO_HAS_CO_AWAIT)
    net::awaitable<void>
    testRebind(net::mutable_buffer& b)
    {
        auto ex = co_await net::this_coro::executor;
        auto s1 = test::stream(ex);
        auto s2 = net::use_awaitable.as_default_on(std::move(s1));
#ifdef BOOST_ASIO_HAS_DEFAULT_FUNCTION_TEMPLATE_ARGUMENTS
        auto bt = co_await s2.async_read_some(b);
        bt = co_await s2.async_write_some(b);
#else
        auto bt = co_await s2.async_read_some(b,
            net::use_awaitable);
        bt = co_await s2.async_write_some(b,
            net::use_awaitable);
#endif
    }
#endif

    void
    run() override
    {
        testTestStream();
        testSharedAbandon();
        //testLifetimeViolation();
        boost::ignore_unused(&stream_test::testAsioSSLCompat);
#if defined(BOOST_ASIO_HAS_CO_AWAIT)
    boost::ignore_unused(&stream_test::testRebind);
#endif
    }
};

BEAST_DEFINE_TESTSUITE(beast,test,stream);

} // beast
} // boost
