// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/mod.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(hana::mod(hana::int_c<6>, hana::int_c<4>) == hana::int_c<2>);
BOOST_HANA_CONSTANT_CHECK(hana::mod(hana::int_c<-6>, hana::int_c<4>) == hana::int_c<-2>);
static_assert(hana::mod(6, 4) == 2, "");

int main() { }
