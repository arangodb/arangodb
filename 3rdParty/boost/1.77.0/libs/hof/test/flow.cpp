/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    flow.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/flow.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/lambda.hpp>
#include <boost/hof/placeholders.hpp>
#include <memory>
#include "test.hpp"

namespace flow_test {
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
    static_assert(noexcept(boost::hof::flow(increment(), decrement(), increment())(3)), "noexcept flow");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::flow(boost::hof::identity)(3) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::flow(boost::hof::identity, boost::hof::identity)(3) == 3);
    BOOST_HOF_TEST_CHECK(boost::hof::flow(boost::hof::identity, boost::hof::identity, boost::hof::identity)(3) == 3);

    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(boost::hof::identity)(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(boost::hof::identity, boost::hof::identity)(3) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(boost::hof::identity, boost::hof::identity, boost::hof::identity)(3) == 3);
}

BOOST_HOF_TEST_CASE()
{
    int r = boost::hof::flow(increment(), decrement(), increment())(3);
    BOOST_HOF_TEST_CHECK(r == 4);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(increment(), decrement(), increment())(3) == 4);
}

BOOST_HOF_TEST_CASE()
{
    int r = boost::hof::flow(increment(), negate(), decrement(), decrement())(3);
    BOOST_HOF_TEST_CHECK(r == -6);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(increment(), negate(), decrement(), decrement())(3) == -6);
}
#ifndef _MSC_VER
BOOST_HOF_TEST_CASE()
{
    constexpr auto f = boost::hof::flow(increment(), decrement());
    static_assert(std::is_empty<decltype(f)>::value, "Compose function not empty");
    int r = f(3);
    BOOST_HOF_TEST_CHECK(r == 3);
    BOOST_HOF_STATIC_TEST_CHECK(f(3) == 3);
}
#endif

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_MOVE_ONLY(increment_movable);
    STATIC_ASSERT_MOVE_ONLY(decrement_movable);
    int r = boost::hof::flow(increment_movable(), decrement_movable(), increment_movable())(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}

BOOST_HOF_TEST_CASE()
{
    const auto f = boost::hof::flow([](int i) { return i+1; }, [](int i) { return i-1; }, [](int i) { return i+1; });
#ifndef _MSC_VER
    static_assert(std::is_empty<decltype(f)>::value, "Compose function not empty");
#endif
    int r = f(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}


BOOST_HOF_STATIC_FUNCTION(f_flow_single_function) = boost::hof::flow(increment());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_flow_single_function(3) == 4);
    BOOST_HOF_STATIC_TEST_CHECK(f_flow_single_function(3) == 4);
}

BOOST_HOF_STATIC_FUNCTION(f_flow_function) = boost::hof::flow(increment(), decrement(), increment());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_flow_function(3) == 4);
    BOOST_HOF_STATIC_TEST_CHECK(f_flow_function(3) == 4);
}

BOOST_HOF_STATIC_FUNCTION(f_flow_function_4) = boost::hof::flow(increment(), negate(), decrement(), decrement());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(f_flow_function_4(3) == -6);
    BOOST_HOF_STATIC_TEST_CHECK(f_flow_function_4(3) == -6);
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(f_flow_lambda) = boost::hof::flow(
    [](int i) { return i+1; }, 
    [](int i) { return i-1; }, 
    [](int i) { return i+1; }
);

BOOST_HOF_TEST_CASE()
{
    int r = f_flow_lambda(3);
    BOOST_HOF_TEST_CHECK(r == 4);
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::flow(boost::hof::_1 + boost::hof::_1, boost::hof::_1 * boost::hof::_1)(3) == 36);
    BOOST_HOF_STATIC_TEST_CHECK(boost::hof::flow(boost::hof::_1 + boost::hof::_1, boost::hof::_1 * boost::hof::_1)(3) == 36);
}
}
