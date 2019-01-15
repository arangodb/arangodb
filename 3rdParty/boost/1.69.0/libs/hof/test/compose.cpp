/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    compose.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/compose.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/lambda.hpp>
#include <boost/hof/placeholders.hpp>
#include <memory>
#include "test.hpp"

namespace compose_test {
struct increment
{
    template<class T>
    constexpr T operator()(T x) const noexcept
    {
        return x + 1;
    }
};

struct decrement
{
    template<class T>
    constexpr T operator()(T x) const noexcept
    {
        return x - 1;
    }
};

struct negate
{
    template<class T>
    constexpr T operator()(T x) const noexcept
    {
        return -x;
    }
};

struct increment_movable
{
    std::unique_ptr<int> n;
    increment_movable()
    : n(new int(1))
    {}
    template<class T>
    T operator()(T x) const
    {
        return x + *n;
    }
};

struct decrement_movable
{
    std::unique_ptr<int> n;
    decrement_movable()
    : n(new int(1))
    {}
    template<class T>
    T operator()(T x) const
    {
        return x - *n;
    }
};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::compose(increment(), decrement(), increment())(3)), "noexcept compose");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::compose(boost::hof::identity)(3) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::compose(boost::hof::identity, boost::hof::identity)(3) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::compose(boost::hof::identity, boost::hof::identity, boost::hof::identity)(3) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(boost::hof::identity)(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(boost::hof::identity, boost::hof::identity)(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(boost::hof::identity, boost::hof::identity, boost::hof::identity)(3) == 3);
}

BOOST_HOF_TEST_CASE()
{
    int r = boost::hof::compose(increment(), decrement(), increment())(3);
    BOOST_HOF_TEST_CHECK(r == 4);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(increment(), decrement(), increment())(3) == 4);
}

BOOST_HOF_TEST_CASE()
{
    int r = boost::hof::compose(increment(), negate(), decrement(), decrement())(3);
    BOOST_HOF_TEST_CHECK(r == 0);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(increment(), negate(), decrement(), decrement())(3) == 0);
}
BOOST_HOF_TEST_CASE()
{
    constexpr auto f = boost::hof::compose(increment(), decrement());
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(f)>::value, "Compose function not empty");
#endif
    static_assert(BOOST_HOF_IS_DEFAULT_CONSTRUCTIBLE(decltype(f)), "Compose function not default constructible");
    int r = f(3);
    BOOST_HOF_TEST_CHECK(r == 3);
    BOOST_HOF_STATIC_TEST_CHECK(f(3) == 3);
}

#ifndef _MSC_VER
BOOST_HOF_TEST_CASE()
{
    constexpr auto f = boost::hof::compose(increment(), negate(), decrement(), decrement());
    static_assert(std::is_empty<decltype(f)>::value, "Compose function not empty");
    static_assert(BOOST_HOF_IS_DEFAULT_CONSTRUCTIBLE(decltype(f)), "Compose function not default constructible");
    int r = f(3);
    BOOST_HOF_TEST_CHECK(r == 0);
    BOOST_HOF_STATIC_TEST_CHECK(f(3) == 0);
}
#endif

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_MOVE_ONLY(increment_movable);
    STATIC_ASSERT_MOVE_ONLY(decrement_movable);
    int r = boost::hof::compose(increment_movable(), decrement_movable(), increment_movable())(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}

template<class T>
struct print;

BOOST_HOF_TEST_CASE()
{
    const auto f = boost::hof::compose([](int i) { return i+1; }, [](int i) { return i-1; }, [](int i) { return i+1; });
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(f)>::value, "Compose function not empty");
#endif
    int r = f(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}


BOOST_HOF_STATIC_FUNCTION(f_compose_single_function) = boost::hof::compose(increment());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_compose_single_function(3) == 4);
    BOOST_HOF_STATIC_TEST_CHECK(f_compose_single_function(3) == 4);
}

BOOST_HOF_STATIC_FUNCTION(f_compose_function) = boost::hof::compose(increment(), decrement(), increment());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_compose_function(3) == 4);
    BOOST_HOF_STATIC_TEST_CHECK(f_compose_function(3) == 4);
}

BOOST_HOF_STATIC_FUNCTION(f_compose_function_4) = boost::hof::compose(increment(), negate(), decrement(), decrement());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_compose_function_4(3) == 0);
    BOOST_HOF_STATIC_TEST_CHECK(f_compose_function_4(3) == 0);
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(f_compose_lambda) = boost::hof::compose(
    [](int i) { return i+1; }, 
    [](int i) { return i-1; }, 
    [](int i) { return i+1; }
);

BOOST_HOF_TEST_CASE()
{
    int r = f_compose_lambda(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::compose(boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1)(3) == 36);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::compose(boost::hof::_1 * boost::hof::_1, boost::hof::_1 + boost::hof::_1)(3) == 36);
}
}
