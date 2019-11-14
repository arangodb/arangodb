// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ap.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/functional/flip.hpp>
#include <boost/hana/lift.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <tuple>
namespace hana = boost::hana;


static_assert(hana::lift<hana::tuple_tag>('x') == hana::make_tuple('x'), "");
static_assert(hana::equal(hana::lift<hana::ext::std::tuple_tag>('x'), std::make_tuple('x')), "");

constexpr auto f = hana::make_pair;
constexpr auto g = hana::flip(hana::make_pair);
static_assert(
    hana::ap(hana::make_tuple(f, g), hana::make_tuple(1, 2, 3), hana::make_tuple('a', 'b'))
        ==
    hana::make_tuple(
        f(1, 'a'), f(1, 'b'), f(2, 'a'), f(2, 'b'), f(3, 'a'), f(3, 'b'),
        g(1, 'a'), g(1, 'b'), g(2, 'a'), g(2, 'b'), g(3, 'a'), g(3, 'b')
    )
, "");

int main() { }
