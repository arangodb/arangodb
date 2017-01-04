// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/not_equal.hpp>

#include <array>
namespace hana = boost::hana;


constexpr std::array<int, 4> xs = {{1, 2, 3, 4}};
constexpr std::array<int, 5> ys = {{1, 2, 3, 4, 5}};

// arrays have different constexpr contents; result is a constexpr bool
static_assert(hana::equal(xs, xs), "");

// arrays have different lengths; result is an integral_constant
BOOST_HANA_CONSTANT_CHECK(hana::not_equal(xs, ys));


int main() { }
