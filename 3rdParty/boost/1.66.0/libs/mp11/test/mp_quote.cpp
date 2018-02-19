
// Copyright 2015, 2017 Peter Dimov.
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt


#include <boost/mp11/utility.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <type_traits>

using boost::mp11::mp_invoke;

template<class...> struct X {};

template<template<class...> class F, class... T> using Y = X<F<T>...>;

template<class Q, class... T> using Z = X<mp_invoke<Q, T>...>;

template<class T, class U> struct P {};

template<class T, class U> using first = T;

int main()
{
    using boost::mp11::mp_identity_t;
    using boost::mp11::mp_quote;

    {
        using Q = mp_quote<mp_identity_t>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, int[]>, int[]>));
    }

    {
        using Q = mp_quote<mp_identity_t>;

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, <= 1800 )
#else
        using R1 = Y<Q::fn, void, char, int>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R1, X<void, char, int>>));
#endif

#if defined( BOOST_MSVC ) && BOOST_WORKAROUND( BOOST_MSVC, < 1920 && BOOST_MSVC >= 1900 )
#else
        using R2 = Z<Q, void, char, int>;
        BOOST_TEST_TRAIT_TRUE((std::is_same<R2, X<void, char, int>>));
#endif
    }

    {
        using Q = mp_quote<P>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void, void>, P<void, void>>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, char[], int[]>, P<char[], int[]>>));
    }

    {
        using Q = mp_quote<first>;

        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, void, int>, void>));
        BOOST_TEST_TRAIT_TRUE((std::is_same<mp_invoke<Q, char[], int[]>, char[]>));
    }

    return boost::report_errors();
}
