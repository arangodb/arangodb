//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/detail/bind_continuation.hpp>

#if 0

#include "test_executor.hpp"
#include "test_handler.hpp"

#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/defer.hpp>
#include <boost/core/exchange.hpp>

namespace boost {
namespace beast {
namespace detail {

class bind_continuation_test
    : public beast::unit_test::suite
{
public:
    class handler
    {
        bool pass_ = false;

    public:
        handler() = default;

        ~handler()
        {
            BEAST_EXPECT(pass_);
        }

        handler(handler&& other)
            : pass_(boost::exchange(other.pass_, true))
        {
        }

        void operator()()
        {
            pass_ = true;
        }
    };

    void
    testBinder()
    {
        net::io_context ioc;
        
        // free functions

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            net::dispatch(
                bind_continuation(ex, handler{}));
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->dispatch == 1);
        }

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            net::post(
                bind_continuation(ex, handler{}));
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->defer == 1);
        }

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            net::defer(
                bind_continuation(ex, handler{}));
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->defer == 1);
        }

        // members

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            ex.dispatch(
                bind_continuation(ex, handler{}),
                std::allocator<void>{});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->dispatch == 1);
        }

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            ex.post(
                bind_continuation(ex, handler{}),
                std::allocator<void>{});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->post == 1);
        }

        {
            test_executor<> ex(ioc.get_executor());
            BEAST_EXPECT(ex->total == 0);
            ex.defer(
                bind_continuation(ex, handler{}),
                std::allocator<void>{});
            ioc.run();
            ioc.restart();
            BEAST_EXPECT(ex->defer == 1);
        }

        // relational

        {
            auto h1 = bind_continuation(
                ioc.get_executor(), handler{});
            auto h2 = bind_continuation(
                ioc.get_executor(), handler{});
            BEAST_EXPECT(
                net::get_associated_executor(h1) ==
                net::get_associated_executor(h2));
            BEAST_EXPECT(
                std::addressof(
                    net::get_associated_executor(h1).context()) ==
                std::addressof(
                    net::get_associated_executor(h2).context()));
            h1();
            h2();
        }

        {
            net::io_context ioc1;
            net::io_context ioc2;
            auto h1 = bind_continuation(
                ioc1.get_executor(), handler{});
            auto h2 = bind_continuation(
                ioc2.get_executor(), handler{});
            BEAST_EXPECT(
                net::get_associated_executor(h1) !=
                net::get_associated_executor(h2));
            BEAST_EXPECT(
                std::addressof(
                    net::get_associated_executor(h1).context()) !=
                std::addressof(
                    net::get_associated_executor(h2).context()));
            h1();
            h2();
        }
    }

    //--------------------------------------------------------------------------

    void
    testJavadoc()
    {
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testBinder();
        testJavadoc();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,bind_continuation);

} // detail
} // beast
} // boost

#endif
