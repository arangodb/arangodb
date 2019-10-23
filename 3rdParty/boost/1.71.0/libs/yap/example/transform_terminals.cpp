// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ transform_terminals
#include <boost/yap/yap.hpp>


struct iota_terminal_transform
{
    // Base case. Note that we're not treating placeholders specially for this
    // example (they're easy to special-case if necessary).
    template<typename T>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::terminal>, T && t)
    {
        // Like the std::iota() algorithm, we create replacement int terminals
        // with the values index_, index_ + 1, index_ + 2, etc.
        return boost::yap::make_terminal(index_++);
    }

    // Recursive case: Match any call expression.
    template<typename CallableExpr, typename... Arg>
    auto operator()(boost::yap::expr_tag<boost::yap::expr_kind::call>,
                    CallableExpr callable, Arg &&... arg)
    {
        // Even though the callable in a call expression is technically a
        // terminal, it doesn't make a lot of sense to replace it with an int,
        // so we'll only transform the argument subexpressions.
        return boost::yap::make_expression<boost::yap::expr_kind::call>(
            boost::yap::as_expr(callable),
            boost::yap::transform(boost::yap::as_expr(arg), *this)...);
    }

    int index_;
};

int sum(int a, int b) { return a + b; }

int main()
{
    {
        // This simple sum(8, 8) expression requires both overloads of
        // iota_terminal_transform.
        auto expr = boost::yap::make_terminal(sum)(8, 8);
        assert(evaluate(expr) == 16);

        auto iota_expr = boost::yap::transform(expr, iota_terminal_transform{1});
        assert(evaluate(iota_expr) == 3);
    }

    {
        // This expression requires only the terminal case of
        // iota_terminal_transform.
        auto expr = -(boost::yap::make_terminal(8) + 8);
        assert(evaluate(expr) == -16);

        auto iota_expr = boost::yap::transform(expr, iota_terminal_transform{0});
        assert(evaluate(iota_expr) == -1);
    }

    {
        // Like the first expression above, this expression requires both
        // overloads of iota_terminal_transform.
        auto expr = boost::yap::make_terminal(sum)(-(boost::yap::make_terminal(8) + 8), 0);
        assert(evaluate(expr) == -16);

        auto iota_expr = boost::yap::transform(expr, iota_terminal_transform{0});
        assert(evaluate(iota_expr) == -3);
    }
}
//]
