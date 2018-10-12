/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    combine.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/combine.hpp>
#include "test.hpp"

#include <boost/hof/construct.hpp>
#include <boost/hof/capture.hpp>
#include <utility>
#include <tuple>

template<class T, class U>
struct mini_pair
{
    T first;
    U second;

    template<class X, class Y>
    constexpr mini_pair(X&& x, Y&& y)
    : first(boost::hof::forward<X>(x)), second(boost::hof::forward<Y>(y))
    {}
};

template<class T1, class U1, class T2, class U2>
constexpr bool operator==(const mini_pair<T1, U1>& x, const mini_pair<T2, U2>& y)
{
    return x.first == y.first && x.second == y.second;
}

template<class T, class U>
constexpr mini_pair<T, U> make_mini_pair(T x, U y)
{
    return mini_pair<T, U>(x, y);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(
        boost::hof::combine(
            boost::hof::construct<std::tuple>(),
            boost::hof::capture_basic(1)(boost::hof::construct<std::pair>()),
            boost::hof::capture_basic(2)(boost::hof::construct<std::pair>())
        )(2, 4) 
        == std::make_tuple(std::make_pair(1, 2), std::make_pair(2, 4)));
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(
        boost::hof::combine(
            boost::hof::construct<mini_pair>(),
            boost::hof::capture_basic(1)(boost::hof::construct<mini_pair>()),
            boost::hof::capture_basic(2)(boost::hof::construct<mini_pair>())
        )(2, 4) 
        == make_mini_pair(make_mini_pair(1, 2), make_mini_pair(2, 4)));

    BOOST_HOF_STATIC_TEST_CHECK(
        boost::hof::combine(
            boost::hof::construct<mini_pair>(),
            boost::hof::capture_basic(1)(boost::hof::construct<mini_pair>()),
            boost::hof::capture_basic(2)(boost::hof::construct<mini_pair>())
        )(2, 4) 
        == make_mini_pair(make_mini_pair(1, 2), make_mini_pair(2, 4)));
}




