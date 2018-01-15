// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/div.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::div(hana::int_c<6>, hana::int_c<3>) == hana::int_c<2>);
BOOST_HANA_CONSTANT_CHECK(hana::div(hana::int_c<6>, hana::int_c<4>) == hana::int_c<1>);

static_assert(hana::div(6, 3) == 2, "");
static_assert(hana::div(6, 4) == 1, "");

int main() { }
