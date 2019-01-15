// Copyright Louis Dionne 2013-2017
// Copyright Jason Rice 2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/core/is_a.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/index_if.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


constexpr auto xs = hana::make_tuple(0, '1', 2.0);

static_assert(hana::index_if(xs, hana::is_an<int>)   == hana::just(hana::size_c<0>), "");
static_assert(hana::index_if(xs, hana::is_a<char>)   == hana::just(hana::size_c<1>), "");
static_assert(hana::index_if(xs, hana::is_a<double>) == hana::just(hana::size_c<2>), "");
static_assert(hana::index_if(xs, hana::is_a<hana::tuple_tag>) == hana::nothing, "");

int main() { }
