// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/yap/expression.hpp>


template<typename T>
using term = boost::yap::terminal<boost::yap::expression, T>;

namespace yap = boost::yap;
namespace bh = boost::hana;


namespace user {

    struct number
    {
        double value;

        friend number operator+(number lhs, number rhs)
        {
            return number{lhs.value + rhs.value};
        }

        friend number operator*(number lhs, number rhs)
        {
            return number{lhs.value * rhs.value};
        }
    };

    // A more efficient fused multiply-add operation would normally go here.
    number naxpy(number a, number x, number y)
    {
        return number{a.value * x.value + y.value};
    }

    // Transforms expressions of the form "a * x + y" to "naxpy(a, x, y)" via
    // the implicit transform customiztion point.
    template<typename Expr1, typename Expr2, typename Expr3>
    decltype(auto) transform_expression(yap::expression<
                                        yap::expr_kind::plus,
                                        bh::tuple<
                                            yap::expression<
                                                yap::expr_kind::multiplies,
                                                bh::tuple<Expr1, Expr2>>,
                                            Expr3>> const & expr)
    {
        return naxpy(
            evaluate(expr.left().left()),
            evaluate(expr.left().right()),
            evaluate(expr.right()));
    }
}

term<user::number> a{{1.0}};
term<user::number> x{{42.0}};
term<user::number> y{{3.0}};

user::number
eval_as_yap_expr(decltype((a * x + y) * (a * x + y) + (a * x + y)) & expr)
{
    return yap::evaluate(expr);
}

user::number eval_as_yap_expr_4x(decltype(
    (a * x + y) * (a * x + y) + (a * x + y) + (a * x + y) * (a * x + y) +
    (a * x + y) + (a * x + y) * (a * x + y) + (a * x + y) +
    (a * x + y) * (a * x + y) + (a * x + y)) & expr)
{
    return yap::evaluate(expr);
}

user::number eval_as_cpp_expr(user::number a, user::number x, user::number y)
{
    return (a * x + y) * (a * x + y) + (a * x + y);
}

user::number eval_as_cpp_expr_4x(user::number a, user::number x, user::number y)
{
    return (a * x + y) * (a * x + y) + (a * x + y) + (a * x + y) * (a * x + y) +
           (a * x + y) + (a * x + y) * (a * x + y) + (a * x + y) +
           (a * x + y) * (a * x + y) + (a * x + y);
}


int main()
{
    auto expr = (a * x + y) * (a * x + y) + (a * x + y);
    user::number result_1 = eval_as_yap_expr(expr);
    user::number result_2 =
        eval_as_cpp_expr(yap::value(a), yap::value(x), yap::value(y));

    (void)result_1;
    (void)result_2;

    return 0;
}
