// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/placeholder.hpp>
namespace hana = boost::hana;


constexpr auto plus = hana::_ + hana::_;
static_assert(plus(1, 2) == 1 + 2, "");

constexpr auto increment = hana::_ + 1;
static_assert(increment(1) == 2, "");

constexpr auto twice = 2 * hana::_;
static_assert(twice(1) == 2, "");

// Extra arguments are ignored.
static_assert(twice(1, "ignored") == 2, "");

int main() { }
