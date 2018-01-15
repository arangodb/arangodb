// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/intersperse.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(hana::intersperse(hana::make_tuple(1, '2', 3.3), 'x') == hana::make_tuple(1, 'x', '2', 'x', 3.3), "");
BOOST_HANA_CONSTANT_CHECK(hana::intersperse(hana::make_tuple(), 'x') == hana::make_tuple());

int main() { }
