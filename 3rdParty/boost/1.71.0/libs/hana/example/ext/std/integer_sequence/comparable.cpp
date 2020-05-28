// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/not_equal.hpp>

#include <utility>
namespace hana = boost::hana;


constexpr std::integer_sequence<int, 1, 2, 3, 4> xs{};
constexpr std::integer_sequence<long, 1, 2, 3, 4> ys{};
constexpr std::integer_sequence<long, 1, 2, 3, 4, 5> zs{};

BOOST_HANA_CONSTANT_CHECK(hana::equal(xs, ys));
BOOST_HANA_CONSTANT_CHECK(hana::not_equal(xs, zs));
BOOST_HANA_CONSTANT_CHECK(hana::not_equal(ys, zs));

int main() { }
