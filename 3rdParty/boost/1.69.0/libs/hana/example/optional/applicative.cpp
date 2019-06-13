// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ap.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>
namespace hana = boost::hana;


constexpr char next(char c) { return c + 1; }

static_assert(hana::ap(hana::just(next), hana::just('x')) == hana::just('y'), "");
BOOST_HANA_CONSTANT_CHECK(hana::ap(hana::nothing, hana::just('x')) == hana::nothing);
BOOST_HANA_CONSTANT_CHECK(hana::ap(hana::just(next), hana::nothing) == hana::nothing);
BOOST_HANA_CONSTANT_CHECK(hana::ap(hana::nothing, hana::nothing) == hana::nothing);

int main() { }
