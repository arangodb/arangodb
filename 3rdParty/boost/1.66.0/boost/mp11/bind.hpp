#ifndef BOOST_MP11_BIND_HPP_INCLUDED
#define BOOST_MP11_BIND_HPP_INCLUDED

//  Copyright 2017 Peter Dimov.
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt

#include <boost/mp11/algorithm.hpp>
#include <cstddef>

namespace boost
{
namespace mp11
{

// mp_arg
template<std::size_t I> struct mp_arg
{
    template<class... T> using fn = mp_at_c<mp_list<T...>, I>;
};

using _1 = mp_arg<0>;
using _2 = mp_arg<1>;
using _3 = mp_arg<2>;
using _4 = mp_arg<3>;
using _5 = mp_arg<4>;
using _6 = mp_arg<5>;
using _7 = mp_arg<6>;
using _8 = mp_arg<7>;
using _9 = mp_arg<8>;

// mp_bind
template<template<class...> class F, class... T> struct mp_bind;

namespace detail
{

template<class V, class... T> struct eval_bound_arg
{
    using type = V;
};

template<std::size_t I, class... T> struct eval_bound_arg<mp_arg<I>, T...>
{
    using type = typename mp_arg<I>::template fn<T...>;
};

template<template<class...> class F, class... U, class... T> struct eval_bound_arg<mp_bind<F, U...>, T...>
{
    using type = typename mp_bind<F, U...>::template fn<T...>;
};

} // namespace detail

template<template<class...> class F, class... T> struct mp_bind
{
    template<class... U> using fn = F<typename detail::eval_bound_arg<T, U...>::type...>;
};

template<class Q, class... T> using mp_bind_q = mp_bind<Q::template fn, T...>;

// mp_bind_front
template<template<class...> class F, class... T> struct mp_bind_front
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 && BOOST_MSVC >= 1900 )
#else
private:
#endif

	template<class... U> struct _fn { using type = F<T..., U...>; };

public:

	// the indirection through _fn works around the language inability
	// to expand U... into a fixed parameter list of an alias template

	template<class... U> using fn = typename _fn<U...>::type;
};

template<class Q, class... T> using mp_bind_front_q = mp_bind_front<Q::template fn, T...>;

// mp_bind_back
template<template<class...> class F, class... T> struct mp_bind_back
{
#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 && BOOST_MSVC >= 1900 )
#else
private:
#endif

	template<class... U> struct _fn { using type = F<U..., T...>; };

public:

	template<class... U> using fn = typename _fn<U...>::type;
};

template<class Q, class... T> using mp_bind_back_q = mp_bind_back<Q::template fn, T...>;

} // namespace mp11
} // namespace boost

#endif // #ifndef BOOST_MP11_BIND_HPP_INCLUDED
