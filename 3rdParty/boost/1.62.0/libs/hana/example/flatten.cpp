// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/flatten.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


static_assert(
    hana::flatten(hana::make_tuple(hana::make_tuple(1, 2, 3),
                                   hana::make_tuple(4, 5),
                                   hana::make_tuple(6, 7, 8, 9)))
    ==
    hana::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9)
, "");

BOOST_HANA_CONSTANT_CHECK(hana::flatten(hana::nothing) == hana::nothing);
static_assert(hana::flatten(hana::just(hana::just(1))) == hana::just(1), "");
BOOST_HANA_CONSTANT_CHECK(hana::flatten(hana::just(hana::nothing)) == hana::nothing);

int main() { }
