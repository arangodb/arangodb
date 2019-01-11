// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/if.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


template <char op>
constexpr auto function = hana::nothing;

template <>
BOOST_HANA_CONSTEXPR_LAMBDA auto function<'+'> = hana::just([](auto x, auto y) {
    return x + y;
});

template <>
BOOST_HANA_CONSTEXPR_LAMBDA auto function<'-'> = hana::just([](auto x, auto y) {
    return x - y;
});

// and so on...

template <char n>
constexpr auto digit = hana::if_(hana::bool_c<(n >= '0' && n <= '9')>,
    hana::just(static_cast<int>(n - 48)),
    hana::nothing
);

template <char x, char op, char y>
BOOST_HANA_CONSTEXPR_LAMBDA auto evaluate = hana::ap(function<op>, digit<x>, digit<y>);

int main() {
    BOOST_HANA_CONSTEXPR_CHECK(evaluate<'1', '+', '2'> == hana::just(1 + 2));
    BOOST_HANA_CONSTEXPR_CHECK(evaluate<'4', '-', '2'> == hana::just(4 - 2));

    BOOST_HANA_CONSTANT_CHECK(evaluate<'?', '+', '2'> == hana::nothing);
    BOOST_HANA_CONSTANT_CHECK(evaluate<'1', '?', '2'> == hana::nothing);
    BOOST_HANA_CONSTANT_CHECK(evaluate<'1', '+', '?'> == hana::nothing);
    BOOST_HANA_CONSTANT_CHECK(evaluate<'?', '?', '?'> == hana::nothing);

    static_assert(hana::lift<hana::optional_tag>(123) == hana::just(123), "");
}
