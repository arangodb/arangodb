// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/yap.hpp>
#include <boost/yap/print.hpp>

#include <iostream>

#include <cmath>


//[ plus_sqrt_term_alias
    template <typename T>
    using term = boost::yap::terminal<boost::yap::expression, T>;
//]
void primer()
{
//[ plus_sqrt_yap_value
//[ plus_sqrt_yap_type
//[ plus_sqrt_yap_top_level_1
    boost::yap::expression<
        boost::yap::expr_kind::plus,
        boost::hana::tuple<
//]
//[ plus_sqrt_yap_lhs
            boost::yap::expression<
                boost::yap::expr_kind::call,
                boost::hana::tuple<
                    boost::yap::expression<
                        boost::yap::expr_kind::terminal,
                        boost::hana::tuple<double (*)(double)>
                    >,
                    boost::yap::expression<
                        boost::yap::expr_kind::terminal,
                        boost::hana::tuple<double>
                    >
                >
            >,
//]
//[ plus_sqrt_yap_rhs
            boost::yap::expression<
                boost::yap::expr_kind::terminal,
                boost::hana::tuple<float>
            >
//]
//[ plus_sqrt_yap_top_level_2
        >
    >
//]
//]
    yap_expr = term<double (*)(double)>{{std::sqrt}}(3.0) + 8.0f;
//]
//[ print_plus_sqrt_yap_value
    print(std::cout, yap_expr);
//]
}

void foo ()
{
//[ assign_through_terminal
    int i = 0;
    auto expr = boost::yap::make_terminal(i) = 42;
    evaluate(expr);
    std::cout << i << "\n"; // Prints 42.
//]
}

//[ print_decl
struct thing {};
//]

void print_expr ()
{
//[ print_expr
using namespace boost::yap::literals;

auto const const_lvalue_terminal_containing_rvalue = boost::yap::make_terminal("lvalue terminal");

double const d = 1.0;
auto rvalue_terminal_containing_lvalue = boost::yap::make_terminal(d);

auto thing_terminal = boost::yap::make_terminal(thing{});

auto expr =
    4_p +
    std::move(rvalue_terminal_containing_lvalue) * thing_terminal -
    const_lvalue_terminal_containing_rvalue;
//]

boost::yap::print(std::cout, expr) << "\n";
}

int main ()
{
    primer();

    foo();

    print_expr();

    return 0;
}
