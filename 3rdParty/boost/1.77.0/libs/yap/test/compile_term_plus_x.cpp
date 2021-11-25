// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <string>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


void compile_term_plus_x()
{
    using namespace std::literals;

    // char const * string
    {
        term<double> unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<char const *>>>
            unevaluated_expr = unity + "3";
        (void)unevaluated_expr;
    }

    // std::string temporary
    {
        term<double> unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<std::string>>>
            unevaluated_expr = unity + "3"s;
        (void)unevaluated_expr;
    }

    // arrays
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int *>>>
            unevaluated_expr = unity + ints;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int const *>>>
            unevaluated_expr = unity + ints;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int *>>>
            unevaluated_expr = unity + std::move(ints);
        (void)unevaluated_expr;
    }

    // pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int *&>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        int const * int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int const *&>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int *>>>
            unevaluated_expr = unity + std::move(int_ptr);
        (void)unevaluated_expr;
    }

    // const pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * const int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int * const &>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        int const * const int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int const * const &>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        int * const int_ptr = ints;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int * const>>>
            unevaluated_expr = unity + std::move(int_ptr);
        (void)unevaluated_expr;
    }

    // values
    {
        term<double> unity{1.0};
        int i = 1;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const i = 1;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int const &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int i = 1;
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int>>>
            unevaluated_expr = unity + std::move(i);
        (void)unevaluated_expr;
    }
}
