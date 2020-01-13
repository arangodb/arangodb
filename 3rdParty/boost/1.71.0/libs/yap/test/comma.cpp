// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <boost/mpl/assert.hpp>

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


struct void_callable
{
    void operator()() { *called_ = (*call_count_)++; }
    int * call_count_;
    int * called_;
};

struct int_callable
{
    int operator()()
    {
        *called_ = (*call_count_)++;
        return 42;
    }
    int * call_count_;
    int * called_;
};

struct double_callable
{
    double operator()()
    {
        *called_ = (*call_count_)++;
        return 13.0;
    }
    int * call_count_;
    int * called_;
};


int test_main(int, char * [])
{
{
    {
        int call_count = 0;
        int int_called = -1;
        int double_called = -1;

        auto int_double_expr =
            (term<int_callable>{{&call_count, &int_called}}(),
             term<double_callable>{{&call_count, &double_called}}());

        BOOST_CHECK(evaluate(int_double_expr) == 13.0);
        BOOST_CHECK(int_called == 0);
        BOOST_CHECK(double_called == 1);
    }

    {
        int call_count = 0;
        int int_called = -1;
        int double_called = -1;

        auto double_int_expr =
            (term<double_callable>{{&call_count, &double_called}}(),
             term<int_callable>{{&call_count, &int_called}}());

        BOOST_CHECK(evaluate(double_int_expr) == 42);
        BOOST_CHECK(int_called == 1);
        BOOST_CHECK(double_called == 0);
    }
}

{
    {
        int call_count = 0;
        int void_called = -1;
        int int_called = -1;

        auto void_int_expr =
            (term<void_callable>{{&call_count, &void_called}}(),
             term<int_callable>{{&call_count, &int_called}}());

        BOOST_CHECK(evaluate(void_int_expr) == 42);
        BOOST_CHECK(void_called == 0);
        BOOST_CHECK(int_called == 1);
    }

    {
        int call_count = 0;
        int void_called = -1;
        int int_called = -1;

        auto int_void_expr =
            (term<int_callable>{{&call_count, &int_called}}(),
             term<void_callable>{{&call_count, &void_called}}());

        using eval_type = decltype(evaluate(int_void_expr));
        BOOST_MPL_ASSERT(
            (std::is_same<void, eval_type>));

        evaluate(int_void_expr);
        BOOST_CHECK(void_called == 1);
        BOOST_CHECK(int_called == 0);
    }
}

return 0;
}
