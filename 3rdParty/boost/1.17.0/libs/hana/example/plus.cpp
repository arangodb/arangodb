// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::plus(hana::int_c<3>, hana::int_c<5>) == hana::int_c<8>);
static_assert(hana::plus(1, 2) == 3, "");
static_assert(hana::plus(1.5f, 2.4) == 3.9, "");

int main() { }
