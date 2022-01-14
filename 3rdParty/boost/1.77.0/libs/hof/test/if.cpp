/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    if.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/if.hpp>
#include "test.hpp"

#include <boost/hof/first_of.hpp>
#include <boost/hof/placeholders.hpp>


struct is_5
{
    template<class T>
    constexpr bool operator()(T i) const
    {
        return i == 5;
    }
};

struct is_not_5
{
    template<class T>
    constexpr bool operator()(T i) const
    {
        return i != 5;
    }
};

template<class F>
struct test_int
{
    template<class T>
    constexpr bool operator()(T x) const
    {
        return boost::hof::first_of(
            boost::hof::if_(std::is_integral<T>())(F()),
            boost::hof::always(true)
        )(x);
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(test_int<is_5>()(5));
    BOOST_HOF_TEST_CHECK(test_int<is_5>()(5L));
    BOOST_HOF_TEST_CHECK(test_int<is_5>()(5.0));
    BOOST_HOF_TEST_CHECK(test_int<is_5>()(6.0));

    BOOST_HOF_TEST_CHECK(test_int<is_not_5>()(6));
    BOOST_HOF_TEST_CHECK(test_int<is_not_5>()(6L));
    BOOST_HOF_TEST_CHECK(test_int<is_not_5>()(5.0));
    BOOST_HOF_TEST_CHECK(test_int<is_not_5>()(6.0));

    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_5>()(5));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_5>()(5L));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_5>()(5.0));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_5>()(6.0));

    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_not_5>()(6));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_not_5>()(6L));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_not_5>()(5.0));
    BOOST_HOF_STATIC_TEST_CHECK(test_int<is_not_5>()(6.0));
}

template<class F>
struct test_int_c
{
    template<class T>
    constexpr bool operator()(T x) const
    {
        return boost::hof::first_of(
            boost::hof::if_c<std::is_integral<T>::value>(F()),
            boost::hof::always(true)
        )(x);
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(test_int_c<is_5>()(5));
    BOOST_HOF_TEST_CHECK(test_int_c<is_5>()(5L));
    BOOST_HOF_TEST_CHECK(test_int_c<is_5>()(5.0));
    BOOST_HOF_TEST_CHECK(test_int_c<is_5>()(6.0));

    BOOST_HOF_TEST_CHECK(test_int_c<is_not_5>()(6));
    BOOST_HOF_TEST_CHECK(test_int_c<is_not_5>()(6L));
    BOOST_HOF_TEST_CHECK(test_int_c<is_not_5>()(5.0));
    BOOST_HOF_TEST_CHECK(test_int_c<is_not_5>()(6.0));

    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_5>()(5));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_5>()(5L));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_5>()(5.0));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_5>()(6.0));

    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_not_5>()(6));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_not_5>()(6L));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_not_5>()(5.0));
    BOOST_HOF_STATIC_TEST_CHECK(test_int_c<is_not_5>()(6.0));
}

struct sum_f
{
    template<class T>
    constexpr int operator()(T x, T y) const
    {
        return boost::hof::first_of(
            boost::hof::if_(std::is_integral<T>())(boost::hof::_ + boost::hof::_),
            boost::hof::always(0)
        )(x, y);
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(sum_f()(1, 2) == 3);
    BOOST_HOF_TEST_CHECK(sum_f()(1.0, 2.0) == 0);
    BOOST_HOF_TEST_CHECK(sum_f()("", "") == 0);

    BOOST_HOF_STATIC_TEST_CHECK(sum_f()(1, 2) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(sum_f()("", "") == 0);
}


struct sum_f_c
{
    template<class T>
    constexpr int operator()(T x, T y) const
    {
        return boost::hof::first_of(
            boost::hof::if_c<std::is_integral<T>::value>(boost::hof::_ + boost::hof::_),
            boost::hof::always(0)
        )(x, y);
    }
};

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK(sum_f_c()(1, 2) == 3);
    BOOST_HOF_TEST_CHECK(sum_f_c()(1.0, 2.0) == 0);
    BOOST_HOF_TEST_CHECK(sum_f_c()("", "") == 0);

    BOOST_HOF_STATIC_TEST_CHECK(sum_f_c()(1, 2) == 3);
    BOOST_HOF_STATIC_TEST_CHECK(sum_f_c()("", "") == 0);
}

#if BOOST_HOF_HAS_NOEXCEPT_DEDUCTION
BOOST_HOF_TEST_CASE()
{
    static_assert(noexcept(boost::hof::if_(std::is_integral<int>())(boost::hof::identity)(1)), "noexcept if");
}
#endif
