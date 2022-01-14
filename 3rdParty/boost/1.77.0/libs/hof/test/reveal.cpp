/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    reveal.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include "test.hpp"
#include <boost/hof/reveal.hpp>
#include <boost/hof/first_of.hpp>
#include <boost/hof/static.hpp>
#include <boost/hof/lambda.hpp>
#include <boost/hof/fix.hpp>

namespace reveal_test {

#define CONDITIONAL_FUNCTION(n) \
struct t ## n {}; \
struct f ## n \
{ \
    constexpr int operator()(t ## n) const \
    { \
        return n; \
    } \
};

CONDITIONAL_FUNCTION(1)
CONDITIONAL_FUNCTION(2)
CONDITIONAL_FUNCTION(3)

typedef boost::hof::first_of_adaptor<f1, f2, f3> f_type;
static constexpr boost::hof::static_<f_type> f = {}; 

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(f)(t1()) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(f)(t2()) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(f)(t3()) == 3);


    static_assert(boost::hof::is_invocable<boost::hof::reveal_adaptor<f_type>, t1>::value, "Invocable");
    static_assert(boost::hof::is_invocable<boost::hof::reveal_adaptor<f_type>, t2>::value, "Invocable");
    static_assert(boost::hof::is_invocable<boost::hof::reveal_adaptor<f_type>, t3>::value, "Invocable");

    static_assert(!boost::hof::is_invocable<boost::hof::reveal_adaptor<f_type>, int>::value, "Invocable");
    // boost::hof::reveal(f)(1);
}

#ifndef _MSC_VER
static constexpr auto lam = boost::hof::first_of(
    BOOST_HOF_STATIC_LAMBDA(t1)
    {
        return 1;
    },
    BOOST_HOF_STATIC_LAMBDA(t2)
    {
        return 2;
    },
    BOOST_HOF_STATIC_LAMBDA(t3)
    {
        return 3;
    }
);

BOOST_HOF_TEST_CASE()
{
    STATIC_ASSERT_EMPTY(lam);
    STATIC_ASSERT_EMPTY(boost::hof::reveal(lam));
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(lam)(t1()) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(lam)(t2()) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(lam)(t3()) == 3);

    // boost::hof::reveal(lam)(1);
    // lam(1);
}
#endif

BOOST_HOF_STATIC_LAMBDA_FUNCTION(static_fun) = boost::hof::first_of(
    [](t1)
    {
        return 1;
    },
    [](t2)
    {
        return 2;
    },
    [](t3)
    {
        return 3;
    }
);

BOOST_HOF_TEST_CASE()
{
#ifndef _MSC_VER
    STATIC_ASSERT_EMPTY(static_fun);
    // STATIC_ASSERT_EMPTY(boost::hof::reveal(static_fun));
#endif
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(static_fun)(t1()) == 1);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(static_fun)(t2()) == 2);
    BOOST_HOF_TEST_CHECK(boost::hof::reveal(static_fun)(t3()) == 3);

    BOOST_HOF_TEST_CHECK(static_fun(t1()) == 1);
    BOOST_HOF_TEST_CHECK(static_fun(t2()) == 2);
    BOOST_HOF_TEST_CHECK(static_fun(t3()) == 3);

    // boost::hof::reveal(static_fun)(1);
}

struct integral_type
{
    template<class T>
    BOOST_HOF_USING_TYPENAME(failure_alias, std::enable_if<std::is_integral<T>::value>::type);

    struct failure
    : boost::hof::as_failure<failure_alias>
    {};

    template<class T, class=typename std::enable_if<std::is_integral<T>::value>::type>
    constexpr T operator()(T x) const
    {
        return x;
    }
};
struct foo {};
struct dont_catch {};

struct catch_all
{
    template<class T>
    BOOST_HOF_USING_TYPENAME(failure_alias, std::enable_if<!std::is_same<T, dont_catch>::value>::type);

    struct failure
    : boost::hof::as_failure<failure_alias>
    {};

    template<class T, class=typename std::enable_if<!std::is_same<T, dont_catch>::value>::type>
    constexpr int operator()(T) const
    {
        return -1;
    }
};

static constexpr boost::hof::reveal_adaptor<boost::hof::first_of_adaptor<integral_type, catch_all>> check_failure = {}; 


BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(check_failure(5) == 5);
    BOOST_HOF_TEST_CHECK(check_failure(foo()) == -1);

    static_assert(!boost::hof::is_invocable<decltype(check_failure), dont_catch>::value, "Invocable");
    static_assert(!boost::hof::is_invocable<decltype(check_failure), int, int>::value, "Invocable");

    // check_failure(dont_catch());
}

}
