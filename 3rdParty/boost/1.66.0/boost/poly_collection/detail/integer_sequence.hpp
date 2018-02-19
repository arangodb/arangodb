/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#ifndef BOOST_POLY_COLLECTION_DETAIL_INTEGER_SEQUENCE_HPP
#define BOOST_POLY_COLLECTION_DETAIL_INTEGER_SEQUENCE_HPP

#if defined(_MSC_VER)
#pragma once
#endif

#include <cstddef>

namespace boost{

namespace poly_collection{

namespace detail{

/* ripped from http://pdimov.com/cpp2/simple_cxx11_metaprogramming.html */

template<typename T,T... Ints> struct integer_sequence{};

template<typename S> struct next_integer_sequence;

template<typename T,T... Ints>
struct next_integer_sequence<integer_sequence<T,Ints...>>
{
  using type=integer_sequence<T,Ints...,sizeof...(Ints)>;
};

template<typename T,T I,T N> struct make_int_seq_impl;

template<typename T,T N>
using make_integer_sequence=typename make_int_seq_impl<T,0,N>::type;

template<typename T,T I,T N> struct make_int_seq_impl
{
  using type=typename next_integer_sequence<
    typename make_int_seq_impl<T,I+1,N>::type>::type;
};

template<typename T,T N> struct make_int_seq_impl<T,N,N>
{
  using type=integer_sequence<T>;
};

template<std::size_t... Ints>
using index_sequence=integer_sequence<std::size_t,Ints...>;

template<std::size_t N>
using make_index_sequence=make_integer_sequence<std::size_t,N>;

} /* namespace poly_collection::detail */

} /* namespace poly_collection */

} /* namespace boost */

#endif
