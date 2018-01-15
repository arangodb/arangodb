//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_WEBSOCKET_DETAIL_PAUSATION_HPP
#define BOOST_BEAST_WEBSOCKET_DETAIL_PAUSATION_HPP

#include <boost/beast/core/handler_ptr.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/assert.hpp>
#include <array>
#include <memory>
#include <new>
#include <utility>

namespace boost {
namespace beast {
namespace websocket {
namespace detail {

// A container that holds a suspended, asynchronous composed
// operation. The contained object may be invoked later to
// resume the operation, or the container may be destroyed.
//
class pausation
{
    struct base
    {
        base() = default;
        base(base &&) = delete;
        base(base const&) = delete;
        virtual ~base() = default;
        virtual void operator()() = 0;
    };

    template<class F>
    struct holder : base
    {
        F f;

        holder(holder&&) = default;

        template<class U>
        explicit
        holder(U&& u)
            : f(std::forward<U>(u))
        {
        }

        void
        operator()() override
        {
            F f_(std::move(f));
            this->~holder();
            // invocation of f_() can
            // assign a new object to *this.
            f_();
        }
    };

    struct exemplar : boost::asio::coroutine
    {
        struct H
        {
            void operator()();
        };

        struct T
        {
            using handler_type = H;
        };

        handler_ptr<T, H> hp;

        void operator()();
    };

    template<class Op>
    class saved_op
    {
        Op* op_ = nullptr;

    public:
        ~saved_op()
        {
            if(op_)
            {
                Op op(std::move(*op_));
                op_->~Op();
                typename std::allocator_traits<
                    boost::asio::associated_allocator_t<Op>>::
                        template rebind_alloc<Op> alloc{
                            boost::asio::get_associated_allocator(op)};
                std::allocator_traits<
                    decltype(alloc)>::deallocate(alloc, op_, 1);
            }
        }

        saved_op(saved_op&& other)
            : op_(other.op_)
        {
            other.op_ = nullptr;
        }

        saved_op& operator=(saved_op&& other)
        {
            BOOST_ASSERT(! op_);
            op_ = other.op_;
            other.op_ = 0;
            return *this;
        }

        explicit
        saved_op(Op&& op)
        {
            typename std::allocator_traits<
                boost::asio::associated_allocator_t<Op>>::
                    template rebind_alloc<Op> alloc{
                        boost::asio::get_associated_allocator(op)};
            auto const p = std::allocator_traits<
                decltype(alloc)>::allocate(alloc, 1);
            op_ = new(p) Op{std::move(op)};
        }

        void
        operator()()
        {
            BOOST_ASSERT(op_);
            Op op{std::move(*op_)};
            typename std::allocator_traits<
                boost::asio::associated_allocator_t<Op>>::
                    template rebind_alloc<Op> alloc{
                        boost::asio::get_associated_allocator(op)};
            std::allocator_traits<
                decltype(alloc)>::deallocate(alloc, op_, 1);
            op_ = nullptr;
            op();
        }
    };

    using buf_type = char[sizeof(holder<exemplar>)];

    base* base_ = nullptr;
    alignas(holder<exemplar>) buf_type buf_;

public:
    pausation() = default;
    pausation(pausation const&) = delete;
    pausation& operator=(pausation const&) = delete;

    ~pausation()
    {
        if(base_)
            base_->~base();
    }

    pausation(pausation&& other)
    {
        boost::ignore_unused(other);
        BOOST_ASSERT(! other.base_);
    }

    pausation&
    operator=(pausation&& other)
    {
        boost::ignore_unused(other);
        BOOST_ASSERT(! base_);
        BOOST_ASSERT(! other.base_);
        return *this;
    }

    template<class F>
    void
    emplace(F&& f);

    template<class F>
    void
    save(F&& f);

    explicit
    operator bool() const
    {
        return base_ != nullptr;
    }

    bool
    maybe_invoke()
    {
        if(base_)
        {
            auto const basep = base_;
            base_ = nullptr;
            (*basep)();
            return true;
        }
        return false;
    }
};

template<class F>
void
pausation::emplace(F&& f)
{
    using type = holder<typename std::decay<F>::type>;
    static_assert(sizeof(buf_type) >= sizeof(type),
        "buffer too small");
    BOOST_ASSERT(! base_);
    base_ = ::new(buf_) type{std::forward<F>(f)};
}

template<class F>
void
pausation::save(F&& f)
{
    emplace(saved_op<F>{std::move(f)});
}

} // detail
} // websocket
} // beast
} // boost

#endif
