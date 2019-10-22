//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_HANDLER_XXX_HPP
#define BOOST_BEAST_TEST_HANDLER_XXX_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/beast/_experimental/unit_test/suite.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/associated_executor.hpp>
#include <boost/asio/handler_alloc_hook.hpp>
#include <boost/asio/handler_continuation_hook.hpp>
#include <boost/asio/handler_invoke_hook.hpp>

namespace boost {
namespace beast {

class simple_allocator
{
    std::size_t id_;

public:
    simple_allocator()
        : id_([]
        {
            static std::size_t n = 0;
            return ++n;
        }())
    {
    }

    friend
    bool operator==(
        simple_allocator const& lhs,
        simple_allocator const& rhs) noexcept
    {
        return lhs.id_ == rhs.id_;
    }

    friend
    bool operator!=(
        simple_allocator const& lhs,
        simple_allocator const& rhs) noexcept
    {
        return lhs.id_ != rhs.id_;
    }
};

class simple_executor
{
    std::size_t id_;

public:
    simple_executor()
        : id_([]
        {
            static std::size_t n = 0;
            return ++n;
        }())
    {
    }

    void* context() { return nullptr; }
    void on_work_started() {}
    void on_work_finished() {}
    template<class F> void dispatch(F&&) {}
    template<class F> void post(F&&) {}
    template<class F> void defer(F&&) {}

    friend
    bool operator==(
        simple_executor const& lhs,
        simple_executor const& rhs) noexcept
    {
        return lhs.id_ == rhs.id_;
    }

    friend
    bool operator!=(
        simple_executor const& lhs,
        simple_executor const& rhs) noexcept
    {
        return lhs.id_ != rhs.id_;
    }
};

// A move-only handler
struct move_only_handler
{
    move_only_handler() = default;
    move_only_handler(move_only_handler&&) = default;
    move_only_handler(move_only_handler const&) = delete;
    void operator()() const{};
};

// Used to test the legacy handler hooks
struct legacy_handler
{
    bool& hook_invoked;

    simple_executor
    get_executor() const noexcept
    {
        return {};
    };

    // Signature of `f` is H<legacy_handler> where H is the wrapper to test
    template<class F>
    static
    void
    test(F const& f)
    {
        {
            bool hook_invoked = false;
            bool lambda_invoked = false;
            auto h = f(legacy_handler{hook_invoked});
            using net::asio_handler_invoke;
            asio_handler_invoke(
                [&lambda_invoked]
                {
                    lambda_invoked =true;
                }, &h);
            BEAST_EXPECT(hook_invoked);
            BEAST_EXPECT(lambda_invoked);
        }
        {
            bool hook_invoked = false;
            auto h = f(legacy_handler{hook_invoked});
            using net::asio_handler_allocate;
            asio_handler_allocate(0, &h);
            BEAST_EXPECT(hook_invoked);
        }
        {
            bool hook_invoked = false;
            auto h = f(legacy_handler{hook_invoked});
            using net::asio_handler_deallocate;
            asio_handler_deallocate(nullptr, 0, &h);
            BEAST_EXPECT(hook_invoked);
        }
        {
            bool hook_invoked = false;
            auto h = f(legacy_handler{hook_invoked});
            using net::asio_handler_is_continuation;
            asio_handler_is_continuation(&h);
            BEAST_EXPECT(hook_invoked);
        }
    }
};

template<class Function>
void
asio_handler_invoke(
    Function&& f,
    legacy_handler* p)
{
    p->hook_invoked = true;
    f();
}

inline
void*
asio_handler_allocate(
    std::size_t,
    legacy_handler* p)
{
    p->hook_invoked = true;
    return nullptr;
}

inline
void
asio_handler_deallocate(
    void*, std::size_t,
    legacy_handler* p)
{
    p->hook_invoked = true;
}

inline
bool
asio_handler_is_continuation(
    legacy_handler* p)
{
    p->hook_invoked = true;
    return false;
}

} // beast
} // boost

namespace boost {
namespace asio {

template<class Allocator>
struct associated_allocator<
    boost::beast::legacy_handler, Allocator>
{
    using type = std::allocator<int>;

    static type get(
        boost::beast::legacy_handler const& h,
        Allocator const& a = Allocator()) noexcept
    {
        return type{};
    }
};

template<class Executor>
struct associated_executor<
    boost::beast::legacy_handler, Executor>
{
    using type = boost::beast::simple_executor;

    static type get(
        boost::beast::legacy_handler const&,
        Executor const& = Executor()) noexcept
    {
        return type{};
    }
};

} // asio
} // boost

#endif
