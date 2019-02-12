// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


void compile_const_term()
{
    {
        term<double const> unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double const> &>, term<int &&>>>
            expr = unity + std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double const> &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<ref<term<double const> &>, term<int &&>>>>>
            unevaluated_expr = unity + std::move(expr);
        (void)unevaluated_expr;
    }

    {
        term<double> const unity{1.0};
        int i_ = 42;
        term<int &&> i{std::move(i_)};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> const &>, term<int &&>>>
            expr = unity + std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double> const &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<ref<term<double> const &>, term<int &&>>>>>
            unevaluated_expr = unity + std::move(expr);
        (void)unevaluated_expr;
    }

    {
        term<double> unity{1.0};
        int i_ = 42;
        term<int const &> i{i_};
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<ref<term<double> &>, term<int const &>>> const expr =
            unity + std::move(i);
        yap::expression<
            yap::expr_kind::plus,
            bh::tuple<
                ref<term<double> &>,
                yap::expression<
                    yap::expr_kind::plus,
                    bh::tuple<ref<term<double> &>, term<int const &>>>>>
            unevaluated_expr = unity + std::move(expr);
        (void)unevaluated_expr;
    }
}
