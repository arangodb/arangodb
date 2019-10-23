/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    construct.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/construct.hpp>
#include "test.hpp"

#include <boost/hof/first_of.hpp>
#include <boost/hof/proj.hpp>
#include <boost/hof/placeholders.hpp>

#include <tuple>
#include <type_traits>
#include <vector>

template<class T>
struct ac
{
    T value;
    constexpr ac(T i) : value(i)
    {}
};

template<class... Ts>
struct tuple_meta
{
    typedef std::tuple<Ts...> type;
};

struct tuple_meta_class
{
    template<class... Ts>
    struct apply
    {
        typedef std::tuple<Ts...> type;
    };
};

struct implicit_default
{
    int mem1;
    std::string mem2;
};
 
struct user_default
{
    int mem1;
    std::string mem2;
    user_default() { }
};

struct user_construct
{
    int mem1;
    std::string mem2;
    user_construct(int) { }
};

template<class T>
struct template_user_construct
{
    int mem1;
    std::string mem2;
    template_user_construct(T) { }
};


BOOST_HOF_TEST_CASE()
{
    auto v = boost::hof::construct<std::vector<int>>()(5, 5);
    BOOST_HOF_TEST_CHECK(v.size() == 5);
    BOOST_HOF_TEST_CHECK(v == std::vector<int>{5, 5, 5, 5, 5});
}

BOOST_HOF_TEST_CASE()
{
    auto v = boost::hof::construct_basic<std::vector<int>>()(5, 5);
    BOOST_HOF_TEST_CHECK(v.size() == 5);
    BOOST_HOF_TEST_CHECK(v == std::vector<int>{5, 5, 5, 5, 5});
}

BOOST_HOF_TEST_CASE()
{
    auto v = boost::hof::construct_forward<std::vector<int>>()(5, 5);
    BOOST_HOF_TEST_CHECK(v.size() == 5);
    BOOST_HOF_TEST_CHECK(v == std::vector<int>{5, 5, 5, 5, 5});
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct<implicit_default>()();
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct<user_default>()();
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct<user_construct>()(3);
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct<template_user_construct>()(3);
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct_forward<template_user_construct>()(3);
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct_basic<template_user_construct>()(3);
    BOOST_HOF_TEST_CHECK(x.mem1 == 0);
    BOOST_HOF_TEST_CHECK(x.mem2 == "");
}

BOOST_HOF_TEST_CASE()
{
    auto v = boost::hof::construct<std::vector<int>>()({5, 5, 5, 5, 5});
    BOOST_HOF_TEST_CHECK(v.size() == 5);
    BOOST_HOF_TEST_CHECK(v == std::vector<int>{5, 5, 5, 5, 5});
}

BOOST_HOF_TEST_CASE()
{
    auto t = boost::hof::construct<std::tuple>()(1, 2, 3);
    static_assert(std::is_same<std::tuple<int, int, int>, decltype(t)>::value, "");
    BOOST_HOF_TEST_CHECK(t == std::make_tuple(1, 2, 3));
// GCC 4.7 doesn't have fully constexpr tuple
#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(std::make_tuple(1, 2, 3) == boost::hof::construct<std::tuple>()(1, 2, 3));
#endif
}

BOOST_HOF_TEST_CASE()
{
    auto t = boost::hof::construct<std::pair>()(1, 2);
    static_assert(std::is_same<std::pair<int, int>, decltype(t)>::value, "");
    BOOST_HOF_TEST_CHECK(t == std::make_pair(1, 2));
// GCC 4.7 doesn't have fully constexpr pair
#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(std::make_pair(1, 2) == boost::hof::construct<std::pair>()(1, 2));
#endif
}

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::first_of(boost::hof::construct<std::pair>(), boost::hof::identity);
    BOOST_HOF_TEST_CHECK(f(1, 2) == std::make_pair(1, 2));
    BOOST_HOF_TEST_CHECK(f(1) == 1);
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct<ac>()(1);
    static_assert(std::is_same<ac<int>, decltype(x)>::value, "");
    BOOST_HOF_TEST_CHECK(x.value == ac<int>(1).value);
    BOOST_HOF_STATIC_TEST_CHECK(ac<int>(1).value == boost::hof::construct<ac>()(1).value);
}

BOOST_HOF_TEST_CASE()
{
    auto x = boost::hof::construct_basic<ac>()(1);
    static_assert(std::is_same<ac<int>, decltype(x)>::value, "");
    BOOST_HOF_TEST_CHECK(x.value == ac<int>(1).value);
    BOOST_HOF_STATIC_TEST_CHECK(ac<int>(1).value == boost::hof::construct<ac>()(1).value);
}

BOOST_HOF_TEST_CASE()
{
    int i = 1;
    auto x = boost::hof::construct_forward<ac>()(i);
    static_assert(std::is_same<ac<int&>, decltype(x)>::value, "");
    BOOST_HOF_TEST_CHECK(&x.value == &i);
}

BOOST_HOF_TEST_CASE()
{
    int i = 1;
    auto x = boost::hof::construct_basic<ac>()(i);
    static_assert(std::is_same<ac<int&>, decltype(x)>::value, "");
    BOOST_HOF_TEST_CHECK(&x.value == &i);
}

BOOST_HOF_TEST_CASE()
{
    auto t = boost::hof::construct_meta<tuple_meta>()(1, 2, 3);
    static_assert(std::is_same<std::tuple<int, int, int>, decltype(t)>::value, "");
    BOOST_HOF_TEST_CHECK(t == std::make_tuple(1, 2, 3));
// GCC 4.7 doesn't have fully constexpr tuple
#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(std::make_tuple(1, 2, 3) == boost::hof::construct_meta<tuple_meta>()(1, 2, 3));
#endif
}

BOOST_HOF_TEST_CASE()
{
    auto t = boost::hof::construct_meta<tuple_meta_class>()(1, 2, 3);
    static_assert(std::is_same<std::tuple<int, int, int>, decltype(t)>::value, "");
    BOOST_HOF_TEST_CHECK(t == std::make_tuple(1, 2, 3));
// GCC 4.7 doesn't have fully constexpr tuple
#if BOOST_HOF_HAS_CONSTEXPR_TUPLE
    BOOST_HOF_STATIC_TEST_CHECK(std::make_tuple(1, 2, 3) == boost::hof::construct_meta<tuple_meta_class>()(1, 2, 3));
#endif
}

