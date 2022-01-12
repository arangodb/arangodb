// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/yap.hpp>

#include <boost/mpl/assert.hpp>

#include <boost/core/lightweight_test.hpp>

#include <sstream>


template<typename T>
using term = boost::yap::terminal<boost::yap::minimal_expr, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


struct iota_terminal_transform
{
    template<typename T>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::terminal>, T && t)
    {
        return boost::yap::make_terminal(index_++);
    }

    template<typename CallableExpr, typename... Arg>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::call>,
                    CallableExpr callable, Arg &&... arg)
    {
        return boost::yap::make_expression<boost::yap::expr_kind::call>(
            callable, boost::yap::transform(arg, *this)...);
    }

    int index_;
};

struct plus_expr_t
{
    static yap::expr_kind const kind = yap::expr_kind::plus;

    bh::tuple<term<int>, term<int>> elements;
};

int main()
{
    // Each node instantiated from from yap::expression.
    {
        auto plus_expr = yap::terminal<yap::expression, int>{{5}} + 6;

        BOOST_TEST(yap::evaluate(plus_expr) == 11);

        BOOST_TEST(
            yap::evaluate(
                yap::transform(plus_expr, iota_terminal_transform{0})) == 1);
    }

    // Each node instantiated from from yap::minimal_expr.
    {
        yap::minimal_expr<yap::expr_kind::plus, bh::tuple<term<int>, term<int>>>
            plus_expr;

        yap::evaluate(yap::transform(plus_expr, iota_terminal_transform{0}), 1);
    }

    // Leaves are instantiated from from yap::minimal_expr; nonterminal
    // expr_kind::plus does not even come from a template.
    {
        plus_expr_t plus_expr;

        yap::evaluate(yap::transform(plus_expr, iota_terminal_transform{0}), 1);
    }

    return boost::report_errors();
}
