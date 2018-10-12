// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/find.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


constexpr auto m = hana::make_map(
    hana::make_pair(hana::type_c<int>, 'i'),
    hana::make_pair(hana::type_c<float>, 'f')
);
static_assert(hana::find(m, hana::type_c<int>) == hana::just('i'), "");
static_assert(hana::find(m, hana::type_c<float>) == hana::just('f'), "");
BOOST_HANA_CONSTANT_CHECK(hana::find(m, hana::type_c<void>) == hana::nothing);
BOOST_HANA_CONSTANT_CHECK(hana::find(m, hana::int_c<3>) == hana::nothing);

// operator[] is equivalent to at_key
static_assert(m[hana::type_c<int>] == 'i', "");
static_assert(m[hana::type_c<float>] == 'f', "");

int main() { }
