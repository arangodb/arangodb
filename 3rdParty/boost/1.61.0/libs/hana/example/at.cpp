// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


constexpr auto xs = hana::make_tuple(0, '1', 2.0);

static_assert(hana::at(xs, hana::size_c<0>) == 0, "");
static_assert(hana::at(xs, hana::size_c<1>) == '1', "");
static_assert(hana::at(xs, hana::size_c<2>) == 2.0, "");

int main() { }
