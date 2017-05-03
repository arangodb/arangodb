// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integral_constant.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/not_equal.hpp>

#include <type_traits>
namespace hana = boost::hana;


static_assert(hana::value(std::integral_constant<int, 3>{}) == 3, "");
static_assert(std::integral_constant<int, 3>::value == 3, "");

BOOST_HANA_CONSTANT_CHECK(hana::equal(std::integral_constant<int, 3>{}, hana::int_c<3>));
BOOST_HANA_CONSTANT_CHECK(hana::equal(std::integral_constant<int, 3>{}, hana::long_c<3>));
BOOST_HANA_CONSTANT_CHECK(hana::not_equal(std::integral_constant<int, 3>{}, hana::int_c<0>));

int main() { }
