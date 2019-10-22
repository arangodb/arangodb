// Copyright 2018 Glen Joseph Fernandes
// (glenjofe@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_CIRCULAR_BUFFER_ALLOCATORS_HPP
#define BOOST_CIRCULAR_BUFFER_ALLOCATORS_HPP

#include <boost/config.hpp>
#if defined(BOOST_NO_CXX11_ALLOCATOR)
#define BOOST_CB_NO_CXX11_ALLOCATOR
#elif defined(BOOST_LIBSTDCXX_VERSION) && (BOOST_LIBSTDCXX_VERSION < 40800)
#define BOOST_CB_NO_CXX11_ALLOCATOR
#endif
#include <limits>
#if !defined(BOOST_CB_NO_CXX11_ALLOCATOR)
#include <memory>
#else
#include <new>
#endif
#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
#include <utility>
#endif

namespace boost {
namespace cb_details {

#if !defined(BOOST_CB_NO_CXX11_ALLOCATOR)
template<class A>
struct allocator_traits
    : std::allocator_traits<A> {
    using typename std::allocator_traits<A>::value_type;
    using typename std::allocator_traits<A>::size_type;

    static size_type max_size(const A&) BOOST_NOEXCEPT {
        return (std::numeric_limits<size_type>::max)() / sizeof(value_type);
    }
};
#else
template<class A>
struct allocator_traits {
    typedef typename A::value_type value_type;
    typedef typename A::pointer pointer;
    typedef typename A::const_pointer const_pointer;
    typedef typename A::difference_type difference_type;
    typedef typename A::size_type size_type;

    static size_type max_size(const A&) BOOST_NOEXCEPT {
        return (std::numeric_limits<size_type>::max)() / sizeof(value_type);
    }

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)
    template<class U, class... Args>
    static void construct(const A&, U* ptr, Args&&... args) {
        ::new((void*)ptr) U(std::forward<Args>(args)...);
    }
#else
    template<class U, class V>
    static void construct(const A&, U* ptr, V&& value) {
        ::new((void*)ptr) U(std::forward<V>(value));
    }
#endif
#else
    template<class U, class V>
    static void construct(const A&, U* ptr, const V& value) {
        ::new((void*)ptr) U(value);
    }

    template<class U, class V>
    static void construct(const A&, U* ptr, V& value) {
        ::new((void*)ptr) U(value);
    }
#endif

    template<class U>
    static void destroy(const A&, U* ptr) {
        (void)ptr;
        ptr->~U();
    }
};
#endif

} // cb_details
} // boost

#endif
