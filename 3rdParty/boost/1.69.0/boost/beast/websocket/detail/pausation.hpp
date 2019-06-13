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

#include <boost/beast/core/detail/allocator.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/assert.hpp>
#include <memory>
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
    struct handler
    {
        handler() = default;
        handler(handler &&) = delete;
        handler(handler const&) = delete;
        virtual ~handler() = default;
        virtual void destroy() = 0;
        virtual void invoke() = 0;
    };

    template<class Handler>
    class impl : public handler
    {
        Handler h_;

    public:
        template<class DeducedHandler>
        impl(DeducedHandler&& h)
            : h_(std::forward<DeducedHandler>(h))
        {
        }

        void
        destroy() override
        {
            Handler h(std::move(h_));
            typename beast::detail::allocator_traits<
                boost::asio::associated_allocator_t<
                    Handler>>::template rebind_alloc<impl> alloc{
                        boost::asio::get_associated_allocator(h)};
            beast::detail::allocator_traits<
                decltype(alloc)>::destroy(alloc, this);
            beast::detail::allocator_traits<
                decltype(alloc)>::deallocate(alloc, this, 1);
        }

        void
        invoke() override
        {
            Handler h(std::move(h_));
            typename beast::detail::allocator_traits<
                boost::asio::associated_allocator_t<
                    Handler>>::template rebind_alloc<impl> alloc{
                        boost::asio::get_associated_allocator(h)};
            beast::detail::allocator_traits<
                decltype(alloc)>::destroy(alloc, this);
            beast::detail::allocator_traits<
                decltype(alloc)>::deallocate(alloc, this, 1);
            h();
        }
    };

    handler* h_ = nullptr;

public:
    pausation() = default;
    pausation(pausation const&) = delete;
    pausation& operator=(pausation const&) = delete;

    ~pausation()
    {
        if(h_)
            h_->destroy();
    }

    pausation(pausation&& other)
    {
        boost::ignore_unused(other);
        BOOST_ASSERT(! other.h_);
    }

    pausation&
    operator=(pausation&& other)
    {
        boost::ignore_unused(other);
        BOOST_ASSERT(! h_);
        BOOST_ASSERT(! other.h_);
        return *this;
    }

    template<class CompletionHandler>
    void
    emplace(CompletionHandler&& handler);

    explicit
    operator bool() const
    {
        return h_ != nullptr;
    }

    bool
    maybe_invoke()
    {
        if(h_)
        {
            auto const h = h_;
            h_ = nullptr;
            h->invoke();
            return true;
        }
        return false;
    }
};

template<class CompletionHandler>
void
pausation::emplace(CompletionHandler&& handler)
{
    BOOST_ASSERT(! h_);
    typename beast::detail::allocator_traits<
        boost::asio::associated_allocator_t<
            CompletionHandler>>::template rebind_alloc<
                impl<CompletionHandler>> alloc{
                    boost::asio::get_associated_allocator(handler)};
    using A = decltype(alloc);
    auto const d =
        [&alloc](impl<CompletionHandler>* p)
        {
            beast::detail::allocator_traits<A>::deallocate(alloc, p, 1);
        };
    std::unique_ptr<impl<CompletionHandler>, decltype(d)> p{
        beast::detail::allocator_traits<A>::allocate(alloc, 1), d};
    beast::detail::allocator_traits<A>::construct(
        alloc, p.get(), std::forward<CompletionHandler>(handler));
    h_ = p.release();
}

} // detail
} // websocket
} // beast
} // boost

#endif
