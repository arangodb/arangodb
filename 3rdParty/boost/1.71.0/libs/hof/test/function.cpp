/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    function.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/function.hpp>
#include <boost/hof/partial.hpp>
#include <boost/hof/infix.hpp>
#include <memory>
#include "test.hpp"

namespace test_constexpr {

struct sum_f
{
    template<class T, class U>
    constexpr T operator()(T x, U y) const
    {
        return x+y;
    }
};

BOOST_HOF_STATIC_FUNCTION(sum_init) = sum_f();

BOOST_HOF_TEST_CASE()
{
// TODO: Should be empty on MSVC as well
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_init);
#endif
    BOOST_HOF_TEST_CHECK(3 == sum_init(1, 2));

    BOOST_HOF_STATIC_TEST_CHECK(3 == sum_init(1, 2));
}

BOOST_HOF_STATIC_FUNCTION(sum_partial) = boost::hof::partial(sum_f());
BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_partial);
#endif
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1, 2));
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1)(2));

    BOOST_HOF_STATIC_TEST_CHECK(3 == sum_partial(1, 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == sum_partial(1)(2));
}

}

namespace test_static {

struct sum_f
{
    template<class T, class U>
    constexpr T operator()(T x, U y) const
    {
        return x+y;
    }
};

struct add_one_f
{
    template<class T>
    constexpr T operator()(T x) const
    {
        return x+1;
    }
};

BOOST_HOF_STATIC_FUNCTION(sum_partial) = boost::hof::partial(sum_f());

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_partial);
#endif
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1, 2));
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1)(2));
}

BOOST_HOF_STATIC_FUNCTION(add_one_pipable) = boost::hof::pipable(add_one_f());

BOOST_HOF_TEST_CASE()
{
// TODO: Make this work on msvc
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(add_one_pipable);
#endif
    BOOST_HOF_TEST_CHECK(3 == add_one_pipable(2));
    BOOST_HOF_TEST_CHECK(3 == (2 | add_one_pipable));
}

BOOST_HOF_STATIC_FUNCTION(sum_infix) = boost::hof::infix(sum_f());

BOOST_HOF_TEST_CASE()
{
// TODO: Make this work on msvc
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_infix);
#endif
    BOOST_HOF_TEST_CHECK(3 == (1 <sum_infix> 2));
}

}
