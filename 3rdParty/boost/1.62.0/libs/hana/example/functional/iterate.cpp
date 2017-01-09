// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/functional/iterate.hpp>
#include <boost/hana/functional/placeholder.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


constexpr auto next_10 = hana::iterate<10>(hana::_ + 1);
static_assert(next_10(3) == 13, "");

constexpr auto xs = hana::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
static_assert(hana::iterate<3>(hana::make_tuple, xs) ==
              hana::make_tuple(hana::make_tuple(hana::make_tuple(xs))), "");

int main() { }
