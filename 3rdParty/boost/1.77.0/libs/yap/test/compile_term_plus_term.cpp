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


void compile_term_plus_term()
{
    using namespace std::literals;

    // char const * string
    {
        term<double> unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<char const *>>>
            unevaluated_expr = unity + term<char const *>{"3"};
        (void)unevaluated_expr;
    }

    // std::string temporary
    {
        term<double> unity{1.0};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<std::string>>>
            unevaluated_expr = unity + term<std::string>{"3"s};
        (void)unevaluated_expr;
    }

    // pointers
    {
        term<double> unity{1.0};
        int ints_[] = {1, 2};
        term<int *> ints = {ints_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int *> &>>>
            unevaluated_expr = unity + ints;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const ints_[] = {1, 2};
        term<int const *> ints = {ints_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int const *> &>>>
            unevaluated_expr = unity + ints;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int ints_[] = {1, 2};
        term<int *> ints = {ints_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int *>>>
            unevaluated_expr = unity + std::move(ints);
        (void)unevaluated_expr;
    }

    // const pointers
    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        term<int * const> int_ptr = {ints};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int * const> &>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int const ints[] = {1, 2};
        term<int const * const> int_ptr = {ints};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int const * const> &>>>
            unevaluated_expr = unity + int_ptr;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int ints[] = {1, 2};
        term<int * const> int_ptr = {ints};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int * const>>>
            unevaluated_expr = unity + std::move(int_ptr);
        (void)unevaluated_expr;
    }

    // values
    {
        term<double> unity{1.0};
        term<int> i = {1};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int> &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        term<int const> i = {1};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int const> &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        term<int> i = {1};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int>>>
            unevaluated_expr = unity + std::move(i);
        (void)unevaluated_expr;
    }

    // const value terminals
    {
        term<double> unity{1.0};
        term<int> const i = {1};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int> const &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        term<int const> const i = {1};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int const> const &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    // lvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int &> &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int const &> i{i_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, ref<term<int const &> &>>>
            unevaluated_expr = unity + i;
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &> i{i_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int &>>>
            unevaluated_expr = unity + std::move(i);
        (void)unevaluated_expr;
    }

    // rvalue refs
    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            unevaluated_expr = unity + std::move(i);
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 1;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int &&>>>
            unevaluated_expr = unity + std::move(i);
        (void)unevaluated_expr;
    }
}
