/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    fix.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/fix.hpp>
#include <boost/hof/static.hpp>
#include <boost/hof/reveal.hpp>
#include <boost/hof/result.hpp>
#include "test.hpp"

#include <memory>

struct factorial_t
{
    template<class Self, class T>
    T operator()(Self s, T x) const noexcept
    {
        return x == 0 ? 1 : x * s(x-1);
    }
};

struct factorial_constexpr_t
{
    template<class Self, class T>
    constexpr T operator()(Self s, T x) const noexcept
    {
        return x == 0 ? 1 : x * s(x-1);
    }
};

struct factorial_move_t
{
    std::unique_ptr<int> i;
    factorial_move_t() : i(new int(1))
    {}
    template<class Self, class T>
    T operator()(const Self& s, T x) const
    {
        return x == 0 ? *i : x * s(x-1);
    }
};

static constexpr boost::hof::fix_adaptor<factorial_t> factorial = {};
static constexpr boost::hof::fix_adaptor<factorial_constexpr_t> factorial_constexpr = {};
static constexpr boost::hof::static_<boost::hof::fix_adaptor<factorial_move_t> > factorial_move = {};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(factorial(5)), "noexcept fix");
}
#endif

BOOST_HOF_TEST_CASE()
{
    const int r = factorial(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
}

BOOST_HOF_TEST_CASE()
{
    const int r = boost::hof::reveal(factorial)(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
}

#if !BOOST_HOF_NO_EXPRESSION_SFINAE
BOOST_HOF_TEST_CASE()
{
    const int r = boost::hof::fix(boost::hof::result<int>(factorial_constexpr_t()))(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
}

BOOST_HOF_TEST_CASE()
{
    const int r = boost::hof::result<int>(factorial_constexpr)(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
}
BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::fix(boost::hof::result<int>(factorial_constexpr_t()))(5) == 5*4*3*2*1);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::result<int>(factorial_constexpr)(5) == 5*4*3*2*1);
}
#endif
BOOST_HOF_TEST_CASE()
{
#if BOOST_HOF_HAS_GENERIC_LAMBDA
    auto factorial_ = boost::hof::fix([](auto s, auto x) -> decltype(x) { return x == 0 ? 1 : x * s(x-1); });
    int r = boost::hof::result<int>(factorial_)(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
#endif
}

BOOST_HOF_TEST_CASE()
{
    const int r = factorial_move(5);
    BOOST_HOF_TEST_CHECK(r == 5*4*3*2*1);
    BOOST_HOF_TEST_CHECK(boost::hof::fix(factorial_move_t())(5) == 5*4*3*2*1);
}
