// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/test/minimal.hpp>

#include <sstream>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using term_ref = boost::yap::expression_ref<boost::yap::expression, term<T> &>;

template<typename T>
using term_cref =
    boost::yap::expression_ref<boost::yap::expression, term<T> const &>;

namespace yap = boost::yap;
namespace bh = boost::hana;


struct callable
{
    int operator()() { return 42; }
};

struct side_effect_callable_1
{
    int operator()()
    {
        *value_ = 1;
        return 0;
    }

    int * value_;
};

struct side_effect_callable_2
{
    int operator()()
    {
        *value_ = 2;
        return 0;
    }

    int * value_;
};


int test_main(int, char * [])
{
    {
        int one = 0;
        int two = 0;

        auto true_nothrow_throw_expr = if_else(
            term<bool>{{true}},
            term<callable>{}(),
            term<side_effect_callable_1>{{&one}}());

        BOOST_CHECK(yap::evaluate(true_nothrow_throw_expr) == 42);
        BOOST_CHECK(one == 0);
        BOOST_CHECK(two == 0);
    }

    {
        int one = 0;
        int two = 0;

        auto false_nothrow_throw_expr = if_else(
            term<bool>{{false}},
            term<callable>{}(),
            term<side_effect_callable_1>{{&one}}());

        BOOST_CHECK(yap::evaluate(false_nothrow_throw_expr) == 0);
        BOOST_CHECK(one == 1);
        BOOST_CHECK(two == 0);
    }

    {
        int one = 0;
        int two = 0;

        auto true_throw1_throw2_expr = if_else(
            term<bool>{{true}},
            term<side_effect_callable_1>{{&one}}(),
            term<side_effect_callable_2>{{&two}}());

        BOOST_CHECK(yap::evaluate(true_throw1_throw2_expr) == 0);
        BOOST_CHECK(one == 1);
        BOOST_CHECK(two == 0);
    }

    {
        int one = 0;
        int two = 0;

        auto false_throw1_throw2_expr = if_else(
            term<bool>{{false}},
            term<side_effect_callable_1>{{&one}}(),
            term<side_effect_callable_2>{{&two}}());

        BOOST_CHECK(yap::evaluate(false_throw1_throw2_expr) == 0);
        BOOST_CHECK(one == 0);
        BOOST_CHECK(two == 2);
    }

    return 0;
}
