// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/basic_tuple.hpp>
#include <boost/hana/core/make.hpp>

#include <type_traits>
namespace hana = boost::hana;


constexpr hana::basic_tuple<int, char, double> xs{1, '2', 3.3};
constexpr auto ys = hana::make<hana::basic_tuple_tag>(1, '2', 3.3);
constexpr auto zs = hana::make_basic_tuple(1, '2', 3.3);

static_assert(std::is_same<decltype(ys), decltype(xs)>::value, "");
static_assert(std::is_same<decltype(zs), decltype(xs)>::value, "");

int main() { }
