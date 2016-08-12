// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/array.hpp>

#include <array>
namespace hana = boost::hana;


constexpr std::array<int, 5> a = {{0, 1, 2, 3, 4}};

static_assert(hana::at_c<2>(a) == 2, "");

static_assert(hana::equal(hana::drop_front(a), std::array<int, 4>{{1, 2, 3, 4}}), "");

int main() { }
