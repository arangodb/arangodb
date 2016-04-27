/*
(c) 2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#ifndef BOOST_ALIGN_DETAIL_ELEMENT_TYPE_HPP
#define BOOST_ALIGN_DETAIL_ELEMENT_TYPE_HPP

#include <boost/config.hpp>

#if !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
#include <type_traits>
#else
#include <cstddef>
#endif

namespace boost {
namespace alignment {
namespace detail {

#if !defined(BOOST_NO_CXX11_HDR_TYPE_TRAITS)
template<class T>
struct element_type {
    typedef typename
        std::remove_cv<typename
        std::remove_all_extents<typename
        std::remove_reference<T>::
        type>::type>::type type;
};
#else
template<class T>
struct remove_reference {
    typedef T type;
};

template<class T>
struct remove_reference<T&> {
    typedef T type;
};

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)
template<class T>
struct remove_reference<T&&> {
    typedef T type;
};
#endif

template<class T>
struct remove_all_extents {
    typedef T type;
};

template<class T>
struct remove_all_extents<T[]> {
    typedef typename remove_all_extents<T>::type type;
};

template<class T, std::size_t N>
struct remove_all_extents<T[N]> {
    typedef typename remove_all_extents<T>::type type;
};

template<class T>
struct remove_const {
    typedef T type;
};

template<class T>
struct remove_const<const T> {
    typedef T type;
};

template<class T>
struct remove_volatile {
    typedef T type;
};

template<class T>
struct remove_volatile<volatile T> {
    typedef T type;
};

template<class T>
struct remove_cv {
    typedef typename remove_volatile<typename
        remove_const<T>::type>::type type;
};

template<class T>
struct element_type {
    typedef typename
        remove_cv<typename
        remove_all_extents<typename
        remove_reference<T>::
        type>::type>::type type;
};
#endif

} /* .detail */
} /* .alignment */
} /* .boost */

#endif
