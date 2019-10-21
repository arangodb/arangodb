// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


namespace yap = boost::yap;

struct alternate_expr_1
{
    static const yap::expr_kind kind = yap::expr_kind::plus;
    boost::hana::tuple<> elements;
};

struct alternate_expr_2
{
    static const yap::expr_kind kind = yap::expr_kind::plus;
    boost::hana::tuple<int, double> elements;
};


struct non_expr_1
{};

struct non_expr_2
{
    boost::hana::tuple<int, double> elements;
};

struct non_expr_3
{
    static const int kind = 0;
    boost::hana::tuple<int, double> elements;
};

struct non_expr_4
{
    int kind;
    boost::hana::tuple<int, double> elements;
};

struct non_expr_5
{
    static const yap::expr_kind kind = yap::expr_kind::plus;
};

struct non_expr_6
{
    static const yap::expr_kind kind = yap::expr_kind::plus;
    int elements;
};


void compile_is_expr()
{
    static_assert(
        yap::is_expr<yap::terminal<yap::expression, double>>::value, "");

    static_assert(
        yap::is_expr<yap::terminal<yap::expression, double> const>::value, "");
    static_assert(
        yap::is_expr<yap::terminal<yap::expression, double> const &>::value,
        "");
    static_assert(
        yap::is_expr<yap::terminal<yap::expression, double> &>::value, "");
    static_assert(
        yap::is_expr<yap::terminal<yap::expression, double> &&>::value, "");

    {
        using namespace yap::literals;
        static_assert(yap::is_expr<decltype(1_p)>::value, "");
    }

    static_assert(
        yap::is_expr<yap::expression<
            yap::expr_kind::unary_plus,
            boost::hana::tuple<yap::terminal<yap::expression, double>>>>::value,
        "");
    static_assert(
        yap::is_expr<yap::expression<
            yap::expr_kind::plus,
            boost::hana::tuple<
                yap::terminal<yap::expression, double>,
                yap::terminal<yap::expression, double>>>>::value,
        "");

    static_assert(yap::is_expr<alternate_expr_1>::value, "");
    static_assert(yap::is_expr<alternate_expr_2>::value, "");

    static_assert(!yap::is_expr<int>::value, "");
    static_assert(!yap::is_expr<non_expr_1>::value, "");
    static_assert(!yap::is_expr<non_expr_2>::value, "");
    static_assert(!yap::is_expr<non_expr_3>::value, "");
    static_assert(!yap::is_expr<non_expr_4>::value, "");
    static_assert(!yap::is_expr<non_expr_5>::value, "");
    static_assert(!yap::is_expr<non_expr_6>::value, "");
}
