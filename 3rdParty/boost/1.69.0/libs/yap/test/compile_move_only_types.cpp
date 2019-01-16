// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>

#include <memory>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

template<typename T>
using ref = boost::yap::expression_ref<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


inline auto double_to_float(term<double> expr)
{
    return term<float>{(float)expr.value()};
}

void compile_move_only_types()
{
    term<double> unity{1.0};
    term<std::unique_ptr<int>> i{new int{7}};
    yap::expression<
        yap::expr_kind::plus,
        bh::tuple<ref<term<double> &>, term<std::unique_ptr<int>>>>
        expr_1 = unity + std::move(i);

    yap::expression<
        yap::expr_kind::plus,
        bh::tuple<
            ref<term<double> &>,
            yap::expression<
                yap::expr_kind::plus,
                bh::tuple<ref<term<double> &>, term<std::unique_ptr<int>>>>>>
        expr_2 = unity + std::move(expr_1);

    auto transformed_expr = transform(std::move(expr_2), double_to_float);
    (void)transformed_expr;
}
