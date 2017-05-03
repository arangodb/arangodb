// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::front(hana::make_tuple(1, '2', 3.3)) == 1, "");
static_assert(hana::drop_front(hana::make_tuple(1, '2', 3.3)) == hana::make_tuple('2', 3.3), "");
BOOST_HANA_CONSTANT_CHECK(!hana::is_empty(hana::make_tuple(1, '2', 3.3)));
BOOST_HANA_CONSTANT_CHECK(hana::is_empty(hana::make_tuple()));

int main() { }
