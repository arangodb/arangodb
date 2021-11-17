/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    returns.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/returns.hpp>
#include "test.hpp"

#if !defined (__GNUC__) || defined (__clang__)
struct add_1
{
    int a;
    add_1() : a(1) {}
    
    BOOST_HOF_RETURNS_CLASS(add_1);
    
    template<class T>
    auto operator()(T x) const 
    BOOST_HOF_RETURNS(x+BOOST_HOF_CONST_THIS->a);
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == add_1()(2));   
}
#endif
// TODO: check noexcept
struct id
{
    template<class T>
    constexpr auto operator()(T x) const BOOST_HOF_RETURNS
    (x);
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(id{}(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(id{}(3) == 3);
}

struct void_ {};
constexpr void_ no_op() { return void_{}; }

struct id_comma
{
    template<class T>
    constexpr auto operator()(T&& x) const BOOST_HOF_RETURNS
    (no_op(), boost::hof::forward<T>(x));
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(id_comma{}(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(id_comma{}(3) == 3);
}
