// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::less(1, 4), "");
BOOST_HANA_CONSTANT_CHECK(!hana::less(hana::int_c<3>, hana::int_c<2>));

// less.than is syntactic sugar
static_assert(hana::all_of(hana::tuple_c<int, 1, 2, 3, 4>, hana::less.than(5)), "");
BOOST_HANA_CONSTANT_CHECK(hana::all_of(hana::tuple_c<int, 1, 2, 3, 4>, hana::less_equal.than(hana::int_c<4>)));

int main() { }
