// Copyright (C) 2016-2018 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//[ let
#include <boost/yap/yap.hpp>

#include <boost/hana/map.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/keys.hpp>

#include <vector>
#include <iostream>


// Here, we introduce special let-placeholders, so we can use them along side
// the normal YAP placeholders without getting them confused.
template<long long I>
struct let_placeholder : boost::hana::llong<I>
{
};

// Replaces each let-terminal with the expression with which it was
// initialized in let().  So in 'let(_a = foo)[ _a + 1 ]', this transform will
// be used on '_a + 1' to replace '_a' with 'foo'.  The map_ member holds the
// mapping of let-placeholders to their initializers.
template<typename ExprMap>
struct let_terminal_transform
{
    // This matches only let-placeholders.  For each one matched, we look up
    // its initializer in map_ and return it.
    template<long long I>
    auto operator()(
        boost::yap::expr_tag<boost::yap::expr_kind::terminal>,
        let_placeholder<I> i)
    {
        // If we have an entry in map_ for this placeholder, return the value
        // of the entry.  Otherwise, pass i through as a terminal.
        if constexpr (boost::hana::contains(
                          decltype(boost::hana::keys(map_))(),
                          boost::hana::llong_c<I>)) {
            return map_[boost::hana::llong_c<I>];
        } else {
            return boost::yap::make_terminal(i);
        }
    }

    ExprMap map_;
};

// As you can see below, let() is an eager function; this template is used for
// its return values.  It contains the mapping from let-placeholders to
// initializer expressions used to transform the expression inside '[]' after
// a let()'.  It also has an operator[]() member function that takes the
// expression inside '[]' and returns a version of it with the
// let-placeholders replaced.
template<typename ExprMap>
struct let_result
{
    template<typename Expr>
    auto operator[](Expr && expr)
    {
        return boost::yap::transform(
            std::forward<Expr>(expr), let_terminal_transform<ExprMap>{map_});
    }

    ExprMap map_;
};

// Processes the expressions passed to let() one at a time, adding each one to
// a Hana map of hana::llong<>s to YAP expressions.
template<typename Map, typename Expr, typename... Exprs>
auto let_impl(Map && map, Expr && expr, Exprs &&... exprs)
{
    static_assert(
        Expr::kind == boost::yap::expr_kind::assign,
        "Expressions passed to let() must be of the form placeholder = Expression");
    if constexpr (sizeof...(Exprs) == 0) {
        using I = typename std::remove_reference<decltype(
            boost::yap::value(boost::yap::left(expr)))>::type;
        auto const i = boost::hana::llong_c<I::value>;
        using map_t = decltype(boost::hana::insert(
            map, boost::hana::make_pair(i, boost::yap::right(expr))));
        return let_result<map_t>{boost::hana::insert(
            map, boost::hana::make_pair(i, boost::yap::right(expr)))};
    } else {
        using I = typename std::remove_reference<decltype(
            boost::yap::value(boost::yap::left(expr)))>::type;
        auto const i = boost::hana::llong_c<I::value>;
        return let_impl(
            boost::hana::insert(
                map, boost::hana::make_pair(i, boost::yap::right(expr))),
            std::forward<Exprs>(exprs)...);
    }
}

// Takes N > 0 expressions of the form 'placeholder = expr', and returns an
// object with an overloaded operator[]().
template<typename Expr, typename... Exprs>
auto let(Expr && expr, Exprs &&... exprs)
{
    return let_impl(
        boost::hana::make_map(),
        std::forward<Expr>(expr),
        std::forward<Exprs>(exprs)...);
}

int main()
{
    // Some handy terminals -- the _a and _b let-placeholders and std::cout as
    // a YAP terminal.
    boost::yap::expression<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<let_placeholder<0>>> const _a;
    boost::yap::expression<
        boost::yap::expr_kind::terminal,
        boost::hana::tuple<let_placeholder<1>>> const _b;
    auto const cout = boost::yap::make_terminal(std::cout);

    using namespace boost::yap::literals;

    {
        auto expr = let(_a = 2)[_a + 1];
        assert(boost::yap::evaluate(expr) == 3);
    }

    {
        auto expr = let(_a = 123, _b = 456)[_a + _b];
        assert(boost::yap::evaluate(expr) == 123 + 456);
    }

    // This prints out "0 0", because 'i' is passed as an lvalue, so its
    // decrement is visible outside the let expression.
    {
        int i = 1;

        boost::yap::evaluate(let(_a = 1_p)[cout << --_a << ' '], i);

        std::cout << i << std::endl;
    }

    // Prints "Hello, World" due to let()'s scoping rules.
    {
        boost::yap::evaluate(
            let(_a = 1_p, _b = 2_p)
            [
                // _a here is an int: 1

                let(_a = 3_p) // hides the outer _a
                [
                    cout << _a << _b // prints "Hello, World"
                ]
            ],
            1, " World", "Hello,"
        );
    }

    std::cout << "\n";

    // Due to the macro-substitution style that this example uses, this prints
    // "3132".  Phoenix's let() prints "312", because it only evaluates '1_p
    // << 3' once.
    {
        boost::yap::evaluate(
            let(_a = 1_p << 3)
            [
                _a << "1", _a << "2"
            ],
            std::cout
        );
    }

    std::cout << "\n";
}
//]
