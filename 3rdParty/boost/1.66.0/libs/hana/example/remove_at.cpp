// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/remove_at.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


constexpr auto xs = hana::make_tuple(0, '1', 2.2, 3u);

static_assert(hana::remove_at(xs, hana::size_c<2>) == hana::make_tuple(0, '1', 3u), "");

int main() { }
