/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    lambda.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/lambda.hpp>
#include <boost/hof/first_of.hpp>
#include <boost/hof/partial.hpp>
#include <boost/hof/infix.hpp>
#include <boost/hof/pipable.hpp>
#include <memory>
#include "test.hpp"


static constexpr auto add_one = BOOST_HOF_STATIC_LAMBDA(int x)
{
    return x + 1;
};

template<class F>
struct wrapper : F
{
    BOOST_HOF_INHERIT_CONSTRUCTOR(wrapper, F)
};

template<class T>
constexpr wrapper<T> wrap(T x)
{
    return x;
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == add_one(2));
}

BOOST_HOF_TEST_CASE()
{
    constexpr auto add_one_again = add_one;
    BOOST_HOF_TEST_CHECK(3 == add_one_again(2));
}

BOOST_HOF_TEST_CASE()
{
    constexpr auto add_one_again = wrap(add_one);
    BOOST_HOF_TEST_CHECK(3 == add_one_again(2));
}

namespace test_static {

BOOST_HOF_STATIC_LAMBDA_FUNCTION(add_one) = [](int x)
{
    return x + 1;
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(add_one(2) == 3);
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(sum_partial) = boost::hof::partial([](int x, int y)
{
    return x + y;
});

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_partial);
#endif
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1, 2));
    BOOST_HOF_TEST_CHECK(3 == sum_partial(1)(2));
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(add_one_pipable) = boost::hof::pipable([](int x)
{
    return x + 1;
});

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(add_one_pipable);
#endif
    BOOST_HOF_TEST_CHECK(3 == add_one_pipable(2));
    BOOST_HOF_TEST_CHECK(3 == (2 | add_one_pipable));
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(sum_infix) = boost::hof::infix([](int x, int y)
{
    return x + y;
});

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(sum_infix);
#endif
    BOOST_HOF_TEST_CHECK(3 == (1 <sum_infix> 2));
}

}
