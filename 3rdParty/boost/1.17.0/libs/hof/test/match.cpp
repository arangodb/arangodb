/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    match.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/match.hpp>
#include <boost/hof/static.hpp>
#include <boost/hof/lambda.hpp>
#include <boost/hof/placeholders.hpp>
#include <boost/hof/limit.hpp>
#include "test.hpp"

#include <memory>

namespace test1
{
    struct int_class
    {
        int operator()(int) const
        {
            return 1;
        }
    };

    struct foo
    {};

    struct foo_class
    {
        foo operator()(foo) const
        {
            return foo();
        }
    };

    static constexpr boost::hof::static_<boost::hof::match_adaptor<int_class, foo_class> > fun = {};

    static_assert(std::is_same<int, decltype(fun(1))>::value, "Failed match");
    static_assert(std::is_same<foo, decltype(fun(foo()))>::value, "Failed match");
}

struct int_class
{
    constexpr int operator()(int) const
    {
        return 1;
    }
};

struct foo
{};

struct foo_class
{
    constexpr int operator()(foo) const
    {
        return 2;
    }
};

static constexpr boost::hof::match_adaptor<int_class, foo_class> fun = {};

BOOST_HOF_TEST_CASE()
{
    
    BOOST_HOF_TEST_CHECK(fun(1) == 1);
    BOOST_HOF_TEST_CHECK(fun(foo()) == 2);

    BOOST_HOF_STATIC_TEST_CHECK(fun(1) == 1);
    BOOST_HOF_STATIC_TEST_CHECK(fun(foo()) == 2);
};

BOOST_HOF_TEST_CASE()
{
    
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(fun)(1) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(fun)(foo()) == 2);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::reveal(fun)(1) == 1);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::reveal(fun)(foo()) == 2);
};

BOOST_HOF_TEST_CASE()
{
    
    constexpr auto lam = boost::hof::match(
        BOOST_HOF_STATIC_LAMBDA(int) { return 1; },
        BOOST_HOF_STATIC_LAMBDA(foo) { return 2; }
    );
    
    BOOST_HOF_TEST_CHECK(lam(1) == 1);
    BOOST_HOF_TEST_CHECK(lam(foo()) == 2);
};

BOOST_HOF_TEST_CASE()
{
    int i = 0;
    auto lam = boost::hof::match(
        [&](int) { return i+1; },
        [&](foo) { return i+2; }
    );
// Disable this check on msvc, since lambdas might be default constructible
#ifndef _MSC_VER
    STATIC_ASSERT_NOT_DEFAULT_CONSTRUCTIBLE(decltype(lam));
#endif
    BOOST_HOF_TEST_CHECK(lam(1) == 1);
    BOOST_HOF_TEST_CHECK(lam(foo()) == 2);
};


BOOST_HOF_TEST_CASE()
{
    struct not_default_constructible
    {
        int i;
        not_default_constructible(int x) : i(x)
        {}
    };
    STATIC_ASSERT_NOT_DEFAULT_CONSTRUCTIBLE(not_default_constructible);
    not_default_constructible ndc = not_default_constructible(0);
    auto lam = boost::hof::match(
        [&](int) { return ndc.i+1; },
        [&](foo) { return ndc.i+2; }
    );
// Disable this check on msvc, since lambdas might be default constructible
#ifndef _MSC_VER
    STATIC_ASSERT_NOT_DEFAULT_CONSTRUCTIBLE(decltype(lam));
#endif
    
    BOOST_HOF_TEST_CHECK(lam(1) == 1);
    BOOST_HOF_TEST_CHECK(lam(foo()) == 2);
};


struct int_move_class
{
    std::unique_ptr<int> i;
    int_move_class() : i(new int(1))
    {}
    int operator()(int) const
    {
        return *i;
    }
};

struct foo_move_class
{
    std::unique_ptr<int> i;
    foo_move_class() : i(new int(2))
    {}
    int operator()(foo) const
    {
        return *i;
    }
};

BOOST_HOF_TEST_CASE()
{
    auto fun_move = boost::hof::match(int_move_class(), foo_move_class());
    BOOST_HOF_TEST_CHECK(fun_move(1) == 1);
    BOOST_HOF_TEST_CHECK(fun_move(foo()) == 2);

};

BOOST_HOF_TEST_CASE()
{
    const auto negate = (!boost::hof::_);
    const auto sub = (boost::hof::_ - boost::hof::_);
    BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0) == negate(0));
    BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0, 1) == sub(0, 1));
}

BOOST_HOF_TEST_CASE()
{
    const auto negate = (!boost::hof::_1);
    const auto sub = (boost::hof::_1 - boost::hof::_2);
    BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0) == negate(0));
    // This is test is disabled because its invalid. It is valid to give more
    // parameters than what are used by the placeholders. So negate(0, 1) is a
    // valid call, which makes the following test fail with ambigous overload.
    // BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0, 1) == sub(0, 1));
}

BOOST_HOF_TEST_CASE()
{
    const auto negate = boost::hof::limit_c<1>(!boost::hof::_1);
    const auto sub = boost::hof::limit_c<2>(boost::hof::_1 - boost::hof::_2);
    BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0) == negate(0));
    // This test will pass because we have limited the number of parameters.
    BOOST_HOF_TEST_CHECK(boost::hof::match(negate, sub)(0, 1) == sub(0, 1));
}

