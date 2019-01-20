// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


BOOST_HANA_CONSTANT_CHECK(
    hana::find(hana::make_tuple(hana::int_c<1>, hana::type_c<int>, '3'), hana::type_c<int>) == hana::just(hana::type_c<int>)
);
BOOST_HANA_CONSTANT_CHECK(
    hana::find(hana::make_tuple(hana::int_c<1>, hana::type_c<int>, '3'), hana::type_c<void>) == hana::nothing
);

constexpr auto m = hana::make_map(
    hana::make_pair(hana::int_c<2>, 2),
    hana::make_pair(hana::type_c<float>, 3.3),
    hana::make_pair(hana::type_c<char>, hana::type_c<int>)
);
static_assert(hana::find(m, hana::type_c<float>) == hana::just(3.3), "");

int main() { }
