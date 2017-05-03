// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/greater_equal.hpp>
#include <boost/hana/integral_constant.hpp>
namespace hana = boost::hana;


static_assert(hana::greater_equal(4, 1), "");
static_assert(hana::greater_equal(1, 1), "");
BOOST_HANA_CONSTANT_CHECK(!hana::greater_equal(hana::int_c<1>, hana::int_c<2>));

int main() { }
