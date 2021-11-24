// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/max.hpp>
namespace hana = boost::hana;


static_assert(hana::max(1, 4) == 4, "");
BOOST_HANA_CONSTANT_CHECK(hana::max(hana::int_c<7>, hana::int_c<5>) == hana::int_c<7>);

int main() { }
