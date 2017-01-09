// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/first.hpp>
#include <boost/hana/functional/on.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


// infix application
constexpr auto sorted = hana::sort.by(hana::less ^hana::on^ hana::first, hana::make_tuple(
    hana::make_pair(hana::int_c<3>, 'x'),
    hana::make_pair(hana::int_c<1>, hana::type_c<void>),
    hana::make_pair(hana::int_c<2>, 9876)
));

static_assert(sorted == hana::make_tuple(
    hana::make_pair(hana::int_c<1>, hana::type_c<void>),
    hana::make_pair(hana::int_c<2>, 9876),
    hana::make_pair(hana::int_c<3>, 'x')
), "");


// function call syntax
constexpr auto x = hana::make_pair(1, 2);
constexpr auto y = hana::make_pair(10, 20);
static_assert(hana::on(hana::plus, hana::first)(x, y) == 1 + 10, "");

int main() { }
