// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/difference.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


constexpr auto xs = hana::make_set(hana::int_c<1>, hana::int_c<2>, hana::type_c<int>, hana::int_c<3>);
constexpr auto ys = hana::make_set(hana::int_c<3>, hana::type_c<void>, hana::type_c<int>);

BOOST_HANA_CONSTANT_CHECK(hana::difference(xs, ys) == hana::make_set(hana::int_c<1>, hana::int_c<2>));
BOOST_HANA_CONSTANT_CHECK(hana::difference(ys, xs) == hana::make_set(hana::type_c<void>));

int main() { }
