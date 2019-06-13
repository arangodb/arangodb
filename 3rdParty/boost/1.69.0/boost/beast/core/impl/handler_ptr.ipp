//
// Copyright (c) 2016-2017 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/boostorg/beast
//

#ifndef BOOST_BEAST_IMPL_HANDLER_PTR_HPP
#define BOOST_BEAST_IMPL_HANDLER_PTR_HPP

#include <boost/asio/associated_allocator.hpp>
#include <boost/assert.hpp>
#include <memory>

namespace boost {
namespace beast {

template<class T, class Handler>
void
handler_ptr<T, Handler>::
clear()
{
    typename beast::detail::allocator_traits<
        boost::asio::associated_allocator_t<
            Handler>>::template rebind_alloc<T> alloc(
                boost::asio::get_associated_allocator(
                    handler()));
    beast::detail::allocator_traits<
        decltype(alloc)>::destroy(alloc, t_);
    beast::detail::allocator_traits<
        decltype(alloc)>::deallocate(alloc, t_, 1);
    t_ = nullptr;
}

template<class T, class Handler>
handler_ptr<T, Handler>::
~handler_ptr()
{
    if(t_)
    {
        clear();
        handler().~Handler();
    }
}

template<class T, class Handler>
handler_ptr<T, Handler>::
handler_ptr(handler_ptr&& other)
    : t_(other.t_)
{
    if(other.t_)
    {
        new(&h_) Handler(std::move(other.handler()));
        other.handler().~Handler();
        other.t_ = nullptr;
    }
}

template<class T, class Handler>
template<class DeducedHandler, class... Args>
handler_ptr<T, Handler>::
handler_ptr(DeducedHandler&& h, Args&&... args)
{
    BOOST_STATIC_ASSERT(! std::is_array<T>::value);
    typename beast::detail::allocator_traits<
        boost::asio::associated_allocator_t<
            Handler>>::template rebind_alloc<T> alloc{
                boost::asio::get_associated_allocator(h)};
    using A = decltype(alloc);
    bool destroy = false;
    auto deleter = [&alloc, &destroy](T* p)
    {
        if(destroy)
            beast::detail::allocator_traits<A>::destroy(alloc, p);
        beast::detail::allocator_traits<A>::deallocate(alloc, p, 1);
    };
    std::unique_ptr<T, decltype(deleter)> t{
        beast::detail::allocator_traits<A>::allocate(alloc, 1), deleter};
    beast::detail::allocator_traits<A>::construct(alloc, t.get(),
        static_cast<DeducedHandler const&>(h),
            std::forward<Args>(args)...);
    destroy = true;
    new(&h_) Handler(std::forward<DeducedHandler>(h));
    t_ = t.release();
}

template<class T, class Handler>
auto
handler_ptr<T, Handler>::
release_handler() ->
    handler_type
{
    BOOST_ASSERT(t_);
    clear();
    auto deleter = [](Handler* h)
    {
        h->~Handler();
    };
    std::unique_ptr<
        Handler, decltype(deleter)> destroyer{
            &handler(), deleter};
    return std::move(handler());
}

template<class T, class Handler>
template<class... Args>
void
handler_ptr<T, Handler>::
invoke(Args&&... args)
{
    BOOST_ASSERT(t_);
    clear();
    auto deleter = [](Handler* h)
    {
        boost::ignore_unused(h); // fix #1119
        h->~Handler();
    };
    std::unique_ptr<
        Handler, decltype(deleter)> destroyer{
            &handler(), deleter};
    handler()(std::forward<Args>(args)...);
}

} // beast
} // boost

#endif
