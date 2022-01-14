//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

// Test that header file is self-contained.
#include <boost/beast/core/bind_handler.hpp>

#include "test_handler.hpp"

#include <boost/beast/core/detail/type_traits.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/beast/_experimental/test/stream.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/defer.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/strand.hpp>
#include <boost/bind/placeholders.hpp>
#include <boost/core/exchange.hpp>
#include <memory>
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

    struct copyable
    {
        template<class... Args>
        void
        operator()(Args&&...) const
        {
        }
    };

    struct move_only
    {
        move_only() = default;
        move_only(move_only&&) = default;
        move_only(move_only const&) = delete;
        void operator()() const{};
    };

    // A move-only parameter
    template<std::size_t I>
    struct move_arg
    {
        move_arg() = default;
        move_arg(move_arg&&) = default;
        move_arg(move_arg const&) = delete;
        void operator()() const
        {
        };
    };

    void
    callback(int v)
    {
        BEAST_EXPECT(v == 42);
    }

    class test_executor
    {
        bind_handler_test& s_;

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
        net::any_io_executor ex_;

        // Storing the blocking property as a member is not strictly necessary,
        // as we could simply forward the calls
        //   require(ex_, blocking.possibly)
        // and
        //   require(ex_, blocking.never)
        // to the underlying executor, and then
        //   query(ex_, blocking)
        // when required. This forwarding approach is used here for the
        // outstanding_work property.
        net::execution::blocking_t blocking_;

#else  // defined(BOOST_ASIO_NO_TS_EXECUTORS)
        net::io_context::executor_type ex_;
#endif // defined(BOOST_ASIO_NO_TS_EXECUTORS)
    public:
        test_executor(
            test_executor const&) = default;

        test_executor(
            bind_handler_test& s,
            net::io_context& ioc)
            : s_(s)
            , ex_(ioc.get_executor())
#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
            , blocking_(net::execution::blocking.possibly)
#endif
        {
        }

        bool operator==(
            test_executor const& other) const noexcept
        {
            return ex_ == other.ex_;
        }

        bool operator!=(
            test_executor const& other) const noexcept
        {
            return ex_ != other.ex_;
        }

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)

        net::execution_context&
        query(
            net::execution::context_t c) const noexcept
        {
            return net::query(ex_, c);
        }

        net::execution::blocking_t
        query(
            net::execution::blocking_t) const noexcept
        {
            return blocking_;
        }

        net::execution::outstanding_work_t
        query(
            net::execution::outstanding_work_t w) const noexcept
        {
            return net::query(ex_, w);
        }

        test_executor
        require(
            net::execution::blocking_t::possibly_t b) const
        {
            test_executor new_ex(*this);
            new_ex.blocking_ = b;
            return new_ex;
        }

        test_executor
        require(
            net::execution::blocking_t::never_t b) const
        {
            test_executor new_ex(*this);
            new_ex.blocking_ = b;
            return new_ex;
        }

        test_executor prefer(net::execution::outstanding_work_t::untracked_t w) const
        {
            test_executor new_ex(*this);
            new_ex.ex_ = net::prefer(ex_, w);
            return new_ex;
        }

        test_executor prefer(net::execution::outstanding_work_t::tracked_t w) const
        {
            test_executor new_ex(*this);
            new_ex.ex_ = net::prefer(ex_, w);
            return new_ex;
        }

        template<class F>
        void execute(F&& f) const
        {
            if (blocking_ == net::execution::blocking.possibly)
            {
                s_.on_invoke();
                net::execution::execute(ex_, std::forward<F>(f));
            }
            else
            {
                // shouldn't be called since the enclosing
                // networking wrapper only uses dispatch
                BEAST_FAIL();
            }
        }
#endif
#if !defined(BOOST_ASIO_NO_TS_EXECUTORS)
        net::execution_context&
        context() const noexcept
        {
            return ex_.context();
        }

        void
        on_work_started() const noexcept
        {
            ex_.on_work_started();
        }

        void
        on_work_finished() const noexcept
        {
            ex_.on_work_finished();
        }

        template<class F, class Alloc>
        void
        dispatch(F&& f, Alloc const& a)
        {
            s_.on_invoke();
            net::execution::execute(
                net::prefer(ex_,
                    net::execution::blocking.possibly,
                    net::execution::allocator(a)),
                std::forward<F>(f));
            // previously equivalent to
            // ex_.dispatch(std::forward<F>(f), a);
        }

        template<class F, class Alloc>
        void
        post(F&& f, Alloc const& a)
        {
            // shouldn't be called since the enclosing
            // networking wrapper only uses dispatch
            BEAST_FAIL();
        }

        template<class F, class Alloc>
        void
        defer(F&& f, Alloc const& a)
        {
            // shouldn't be called since the enclosing
            // networking wrapper only uses dispatch
            BEAST_FAIL();
        }
#endif // !defined(BOOST_ASIO_NO_TS_EXECUTORS)
    };

#if defined(BOOST_ASIO_NO_TS_EXECUTORS)
    BOOST_STATIC_ASSERT(net::execution::is_executor<test_executor>::value);
#else
    BOOST_STATIC_ASSERT(net::is_executor<test_executor>::value);
#endif

    class test_cb
    {
        bool fail_ = true;
    
    public:
        test_cb() = default;
        test_cb(test_cb const&) = delete;
        test_cb(test_cb&& other)
            : fail_(boost::exchange(
                other.fail_, false))
        {
        }

        ~test_cb()
        {
            BEAST_EXPECT(! fail_);
        }

        void
        operator()()
        {
            fail_ = false;
            BEAST_EXPECT(true);
        }

        void
        operator()(int v)
        {
            fail_ = false;
            BEAST_EXPECT(v == 42);
        }

        void
        operator()(int v, string_view s)
        {
            fail_ = false;
            BEAST_EXPECT(v == 42);
            BEAST_EXPECT(s == "s");
        }

        void
        operator()(int v, string_view s, move_arg<1>)
        {
            fail_ = false;
            BEAST_EXPECT(v == 42);
            BEAST_EXPECT(s == "s");
        }

        void
        operator()(int v, string_view s, move_arg<1>, move_arg<2>)
        {
            fail_ = false;
            BEAST_EXPECT(v == 42);
            BEAST_EXPECT(s == "s");
        }

        void
        operator()(error_code, std::size_t n)
        {
            fail_ = false;
            BEAST_EXPECT(n == 256);
        }

        void
        operator()(
            error_code, std::size_t n, string_view s)
        {
            boost::ignore_unused(s);
            fail_ = false;
            BEAST_EXPECT(n == 256);
        }

        void
        operator()(std::shared_ptr<int> const& sp)
        {
            fail_ = false;
            BEAST_EXPECT(sp.get() != nullptr);
        }
    };

#if 0
    // These functions should fail to compile

    void
    failStdBind()
    {
        std::bind(bind_handler(test_cb{}));
    }

    void
    failStdBindFront()
    {
        std::bind(bind_front_handler(test_cb{}));
    }
#endif

    //--------------------------------------------------------------------------

    bool invoked_;

    void
    on_invoke()
    {
        invoked_ = true;
    }

    template<class F>
    void
    testHooks(net::io_context& ioc, F&& f)
    {
        invoked_ = false;
        net::post(ioc, std::forward<F>(f));
        ioc.run();
        ioc.restart();
        BEAST_EXPECT(invoked_);
    }

    //--------------------------------------------------------------------------

    void
    testBindHandler()
    {
        using m1 = move_arg<1>;
        using m2 = move_arg<2>;

        {
            using namespace std::placeholders;

            // 0-ary
            bind_handler(test_cb{})();

            // 1-ary
            bind_handler(test_cb{}, 42)(); 
            bind_handler(test_cb{}, _1)(42);
            bind_handler(test_cb{}, _2)(0, 42);

            // 2-ary
            bind_handler(test_cb{}, 42, "s")();
            bind_handler(test_cb{}, 42, "s")(0);
            bind_handler(test_cb{}, _1, "s")(42);
            bind_handler(test_cb{}, 42, _1) ("s");
            bind_handler(test_cb{}, _1, _2)(42, "s");
            bind_handler(test_cb{}, _1, _2)(42, "s", "X");
            bind_handler(test_cb{}, _2, _1)("s", 42);
            bind_handler(test_cb{}, _3, _2)("X", "s", 42);

            // 3-ary
            bind_handler(test_cb{}, 42, "s")(m1{});
            bind_handler(test_cb{}, 42, "s", _1)(m1{});
            bind_handler(test_cb{}, 42, _1, m1{})("s");

            // 4-ary
            bind_handler(test_cb{}, 42, "s")(m1{}, m2{});
            bind_handler(test_cb{}, 42, "s", m1{})(m2{});
            bind_handler(test_cb{}, 42, "s", m1{}, m2{})();
            bind_handler(test_cb{}, 42, _1, m1{})("s", m2{});
            bind_handler(test_cb{}, _3, _1, m1{})("s", m2{}, 42);
        }

        {
            using namespace boost::placeholders;

            // 0-ary
            bind_handler(test_cb{})();

            // 1-ary
            bind_handler(test_cb{}, 42)(); 
            bind_handler(test_cb{}, _1)(42);
            bind_handler(test_cb{}, _2)(0, 42);

            // 2-ary
            bind_handler(test_cb{}, 42, "s")();
            bind_handler(test_cb{}, 42, "s")(0);
            bind_handler(test_cb{}, _1, "s")(42);
            bind_handler(test_cb{}, 42, _1) ("s");
            bind_handler(test_cb{}, _1, _2)(42, "s");
            bind_handler(test_cb{}, _1, _2)(42, "s", "X");
            bind_handler(test_cb{}, _2, _1)("s", 42);
            bind_handler(test_cb{}, _3, _2)("X", "s", 42);

            // 3-ary
            bind_handler(test_cb{}, 42, "s")(m1{});
            bind_handler(test_cb{}, 42, "s", _1)(m1{});
            bind_handler(test_cb{}, 42, _1, m1{})("s");

            // 4-ary
            bind_handler(test_cb{}, 42, "s")(m1{}, m2{});
            bind_handler(test_cb{}, 42, "s", m1{})(m2{});
            bind_handler(test_cb{}, 42, "s", m1{}, m2{})();
            bind_handler(test_cb{}, 42, _1, m1{})("s", m2{});
            bind_handler(test_cb{}, _3, _1, m1{})("s", m2{}, 42);
        }

        // perfect forwarding
        {
            std::shared_ptr<int> const sp =
                std::make_shared<int>(42);
            {
                bind_handler(test_cb{}, sp)();
                BEAST_EXPECT(sp.get() != nullptr);
            }
            {
                bind_handler(test_cb{})(sp);
                BEAST_EXPECT(sp.get() != nullptr);
            }
        }

        // associated executor
        {
            net::io_context ioc;
            testHooks(ioc, bind_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{})));
        }

        // asio_handler_invoke
        {
            // make sure things compile, also can set a
            // breakpoint in asio_handler_invoke to make sure
            // it is instantiated.
            net::io_context ioc;
            net::strand<
                net::io_context::executor_type> s{
                    ioc.get_executor()};
            net::post(s,
                bind_handler(test_cb{}, 42));
            ioc.run();
        }

        // legacy hooks
        legacy_handler::test(
            [](legacy_handler h)
            {
                return bind_handler(h);
            });
    }

    void
    testBindFrontHandler()
    {
        using m1 = move_arg<1>;
        using m2 = move_arg<2>;

        // 0-ary
        bind_front_handler(test_cb{})();

        // 1-ary
        bind_front_handler(test_cb{}, 42)(); 
        bind_front_handler(test_cb{})(42);

        // 2-ary
        bind_front_handler(test_cb{}, 42, "s")();
        bind_front_handler(test_cb{}, 42)("s");
        bind_front_handler(test_cb{})(42, "s");

        // 3-ary
        bind_front_handler(test_cb{}, 42, "s", m1{})();
        bind_front_handler(test_cb{}, 42, "s")(m1{});
        bind_front_handler(test_cb{}, 42)("s", m1{});
        bind_front_handler(test_cb{})(42, "s", m1{});

        // 4-ary
        bind_front_handler(test_cb{}, 42, "s", m1{}, m2{})();
        bind_front_handler(test_cb{}, 42, "s", m1{})(m2{});
        bind_front_handler(test_cb{}, 42, "s")(m1{}, m2{});
        bind_front_handler(test_cb{}, 42)("s", m1{}, m2{});
        bind_front_handler(test_cb{})(42, "s", m1{}, m2{});

        error_code ec;
        std::size_t n = 256;
        
        // void(error_code, size_t)
        bind_front_handler(test_cb{}, ec, n)();

        // void(error_code, size_t)(string_view)
        bind_front_handler(test_cb{}, ec, n)("s");

        // perfect forwarding
        {
            std::shared_ptr<int> const sp =
                std::make_shared<int>(42);
            bind_front_handler(test_cb{}, sp)();
            BEAST_EXPECT(sp.get() != nullptr);
        }

        // associated executor
        {
            net::io_context ioc;

            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{})
                ));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{}),
                42));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{}),
                42, "s"));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{}),
                42, "s", m1{}));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{}),
                42, "s", m1{}, m2{}));
            testHooks(ioc, bind_front_handler(net::bind_executor(
                test_executor(*this, ioc), test_cb{}),
                ec, n));
        }

        // legacy hooks
        legacy_handler::test(
            [](legacy_handler h)
            {
                return bind_front_handler(h);
            });
        legacy_handler::test(
            [](legacy_handler h)
            {
                return bind_front_handler(
                    h, error_code{}, std::size_t{});
            });
    }


    //--------------------------------------------------------------------------

    template <class AsyncReadStream, class ReadHandler>
    void
    signal_aborted (AsyncReadStream& stream, ReadHandler&& handler)
    {
        net::post(
            stream.get_executor(),
            bind_handler (std::forward <ReadHandler> (handler),
                net::error::operation_aborted, 0));
    }

    template <class AsyncReadStream, class ReadHandler>
    void
    signal_eof (AsyncReadStream& stream, ReadHandler&& handler)
    {
        net::post(
            stream.get_executor(),
            bind_front_handler (std::forward<ReadHandler> (handler),
                net::error::eof, 0));
    }

    void
    testJavadocs()
    {
        BEAST_EXPECT((
            &bind_handler_test::signal_aborted<
                test::stream, handler<error_code, std::size_t>>));
            
        BEAST_EXPECT((
            &bind_handler_test::signal_eof<
                test::stream, handler<error_code, std::size_t>>));
    }

    //--------------------------------------------------------------------------

    void
    run() override
    {
        testBindHandler();
        testBindFrontHandler();
        testJavadocs();
    }
};

BEAST_DEFINE_TESTSUITE(beast,core,bind_handler);

} // beast
} // boost
