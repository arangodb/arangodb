//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/bind_handler.hpp>

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/experimental/test/stream.hpp>
#include <boost/beast/unit_test/suite.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <string>

namespace boost {
namespace beast {

class bind_handler_test : public unit_test::suite
{
public:
    template<class... Args>
    struct handler
    {
        void
        operator()(Args const&...) const
        {
        }
    };

#if 0
    // This function should fail to compile
    void
    failStdBind()
    {
        std::bind(bind_handler(handler<>{}));
    }
#endif

    void
    callback(int v)
    {
        BEAST_EXPECT(v == 42);
    }
    
    void
    testPlaceholders()
    {
        namespace ph = std::placeholders;
        
        bind_handler(handler<>{})();
        bind_handler(handler<int>{}, 1)();
        bind_handler(handler<int, std::string>{}, 1, "hello")();
        bind_handler(handler<int>{}, ph::_1)(1);
        bind_handler(handler<int, std::string>{}, ph::_1, ph::_2)(1, "hello");
    }

    struct copyable_handler
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    void
    testAsioHandlerInvoke()
    {
        // make sure things compile, also can set a
        // breakpoint in asio_handler_invoke to make sure
        // it is instantiated.
        boost::asio::io_context ioc;
        boost::asio::io_service::strand s{ioc};
        test::stream ts{ioc};
        boost::asio::post(ioc.get_executor(),
            s.wrap(copyable_handler{}));
    }

    struct move_only
    {
        move_only() = default;
        move_only(move_only&&) = default;
        move_only(move_only const&) = delete;
    };

    void
    testMoveOnly()
    {
        bind_handler([](move_only){}, move_only{})();
        bind_handler([](move_only){}, std::placeholders::_1)(move_only{});
        bind_handler([](move_only, move_only){}, move_only{}, std::placeholders::_1)(move_only{});
    }

    void
    run() override
    {
        auto f = bind_handler(std::bind(
            &bind_handler_test::callback, this,
                std::placeholders::_1), 42);
        f();
        testPlaceholders();
        testAsioHandlerInvoke();
        testMoveOnly();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,bind_handler);

} // beast
} // boost
