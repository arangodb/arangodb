// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ calc3
#include <boost/yap/expression.hpp>

#include <boost/hana/maximum.hpp>

#include <iostream>


// Look! A transform!  This one transforms the expression tree into the arity
// of the expression, based on its placeholders.
//[ calc3_get_arity_xform
struct get_arity
{
    // Base case 1: Match a placeholder terminal, and return its arity as the
    // result.
    template <long long I>
    boost::hana::llong<I> operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
                                      boost::yap::placeholder<I>)
    { return boost::hana::llong_c<I>; }

    // Base case 2: Match any other terminal.  Return 0; non-placeholders do
    // not contribute to arity.
    template <typename T>
    auto operator() (boost::yap::expr_tag<boost::yap::expr_kind::terminal>, T &&)
    {
        using namespace boost::hana::literals;
        return 0_c;
    }

    // Recursive case: Match any expression not covered above, and return the
    // maximum of its children's arities.
    template <boost::yap::expr_kind Kind, typename... Arg>
    auto operator() (boost::yap::expr_tag<Kind>, Arg &&... arg)
    {
        return boost::hana::maximum(
            boost::hana::make_tuple(
                boost::yap::transform(
                    boost::yap::as_expr(std::forward<Arg>(arg)),
                    get_arity{}
                )...
            )
        );
    }
};
//]

int main ()
{
    using namespace boost::yap::literals;

    // These lambdas wrap our expressions as callables, and allow us to check
    // the arity of each as we call it.

    auto expr_1 = 1_p + 2.0;

    auto expr_1_fn = [expr_1](auto &&... args) {
        auto const arity = boost::yap::transform(expr_1, get_arity{});
        static_assert(arity.value == sizeof...(args), "Called with wrong number of args.");
        return evaluate(expr_1, args...);
    };

    auto expr_2 = 1_p * 2_p;

    auto expr_2_fn = [expr_2](auto &&... args) {
        auto const arity = boost::yap::transform(expr_2, get_arity{});
        static_assert(arity.value == sizeof...(args), "Called with wrong number of args.");
        return evaluate(expr_2, args...);
    };

    auto expr_3 = (1_p - 2_p) / 2_p;

    auto expr_3_fn = [expr_3](auto &&... args) {
        auto const arity = boost::yap::transform(expr_3, get_arity{});
        static_assert(arity.value == sizeof...(args), "Called with wrong number of args.");
        return evaluate(expr_3, args...);
    };

    // Displays "5"
    std::cout << expr_1_fn(3.0) << std::endl;

    // Displays "6"
    std::cout << expr_2_fn(3.0, 2.0) << std::endl;

    // Displays "0.5"
    std::cout << expr_3_fn(3.0, 2.0) << std::endl;

    // Static-asserts with "Called with wrong number of args."
    //std::cout << expr_3_fn(3.0) << std::endl;

    // Static-asserts with "Called with wrong number of args."
    //std::cout << expr_3_fn(3.0, 2.0, 1.0) << std::endl;

    return 0;
}
//]
