// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/less.hpp>

#include <array>
namespace hana = boost::hana;


constexpr std::array<int, 4> evens = {{2, 4, 6, 8}};
constexpr std::array<int, 4> odds = {{1, 3, 5, 7}};

constexpr std::array<int, 5> up_to_5 = {{1, 2, 3, 4, 5}};

// arrays with same length
static_assert(hana::less(odds, evens), "");

// arrays with different lenghts
static_assert(hana::less(up_to_5, odds), "");

int main() { }
