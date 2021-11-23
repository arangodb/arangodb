//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_TEST_TEST_EXECUTOR_HPP
#define BOOST_BEAST_TEST_TEST_EXECUTOR_HPP

#include <boost/beast/core/detail/config.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/io_context.hpp>
#include <memory>

namespace boost {
namespace beast {

template<class Executor = net::io_context::executor_type>
class test_executor
{
public:
    // VFALCO These need to be atomic or something
    struct info
    {
        int dispatch = 0;
        int post = 0;
        int defer = 0;
        int work = 0;
        int total = 0;
    };

private:
    struct state
    {
        Executor ex;
        info info_;

        state(Executor const& ex_)
            : ex(ex_)
        {
        }
    };

    std::shared_ptr<state> sp_;

public:
    test_executor(test_executor const&) = default;
    test_executor& operator=(test_executor const&) = default;

    explicit
    test_executor(Executor const& ex)
        : sp_(std::make_shared<state>(ex))
    {
    }

    decltype(sp_->ex.context())
    context() const noexcept
    {
        return sp_->ex.context();
    }

    info&
    operator*() noexcept
    {
        return sp_->info_;
    }

    info*
    operator->() noexcept
    {
        return &sp_->info_;
    }

    void
    on_work_started() const noexcept
    {
        ++sp_->info_.work;
    }

    void
    on_work_finished() const noexcept
    {
    }

    template<class F, class A>
    void
    dispatch(F&& f, A const& a) const
    {
        ++sp_->info_.dispatch;
        ++sp_->info_.total;
        sp_->ex.dispatch(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    post(F&& f, A const& a) const
    {
        ++sp_->info_.post;
        ++sp_->info_.total;
        sp_->ex.post(
            std::forward<F>(f), a);
    }

    template<class F, class A>
    void
    defer(F&& f, A const& a) const
    {
        ++sp_->info_.defer;
        ++sp_->info_.total;
        sp_->ex.defer(
            std::forward<F>(f), a);
    }
};

} // beast
} // boost

#endif
