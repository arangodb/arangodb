/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    infix.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/infix.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/lambda.hpp>
#include <boost/hof/pipable.hpp>
#include <boost/hof/placeholders.hpp>
#include "test.hpp"

struct sum_f
{
    template<class T, class U>
    constexpr T operator()(T x, U y) const BOOST_HOF_RETURNS_DEDUCE_NOEXCEPT(x+y)
    {
        return x+y;
    }
};

static constexpr boost::hof::infix_adaptor<sum_f> sum = {};

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(1 <sum> 2), "noexcept infix");
    static_assert(!noexcept(std::string() <sum> std::string()), "noexcept infix");
}
#endif

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum> 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (1 <sum> 2));
    
    BOOST_HOF_TEST_CHECK(3 == (sum(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (sum(1, 2)));
}

BOOST_HOF_STATIC_FUNCTION(sum1) = boost::hof::infix(sum_f());

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum1> 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (1 <sum1> 2));

    BOOST_HOF_TEST_CHECK(3 == (sum1(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (sum1(1, 2)));
}

BOOST_HOF_STATIC_LAMBDA_FUNCTION(sum2) = boost::hof::infix([](int x, int y) { return x + y; });

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum2> 2));

    BOOST_HOF_TEST_CHECK(3 == (sum2(1, 2)));
}

BOOST_HOF_STATIC_FUNCTION(sum3) = boost::hof::infix(boost::hof::_ + boost::hof::_);

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum3> 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (1 <sum3> 2));

    BOOST_HOF_TEST_CHECK(3 == (sum3(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (sum3(1, 2)));
}


BOOST_HOF_STATIC_LAMBDA_FUNCTION(sum4) = boost::hof::infix(boost::hof::infix([](int x, int y) { return x + y; }));

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum4> 2));

    BOOST_HOF_TEST_CHECK(3 == (sum4(1, 2)));
}

BOOST_HOF_STATIC_FUNCTION(sum5) = boost::hof::infix(boost::hof::infix(boost::hof::_ + boost::hof::_));

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(3 == (1 <sum5> 2));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (1 <sum5> 2));

    BOOST_HOF_TEST_CHECK(3 == (sum5(1, 2)));
    BOOST_HOF_STATIC_TEST_CHECK(3 == (sum5(1, 2)));
}

BOOST_HOF_TEST_CASE()
{
#if (defined(__GNUC__) && !defined (__clang__))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
#endif
    BOOST_HOF_TEST_CHECK(6 == (1 + 2 <sum> 3));
    BOOST_HOF_TEST_CHECK(3 == 1 <sum> 2);
#if (defined(__GNUC__) && !defined (__clang__))
#pragma GCC diagnostic pop
#endif
}

struct foo {};

BOOST_HOF_TEST_CASE()
{
    auto f = boost::hof::infix([](int, int) { return foo{}; });
    auto g = boost::hof::infix([](foo, foo) { return std::string("hello"); });
    BOOST_HOF_TEST_CHECK((1 <f> 2 <g> foo{}) == "hello");

}
