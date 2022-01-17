// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


int main() {
    // Given a map of (key, value) pairs, returns a map of (value, key) pairs.
    // This requires both the keys and the values to be compile-time Comparable.
    auto invert = [](auto map) {
        return hana::fold_left(map, hana::make_map(), [](auto map, auto pair) {
            return hana::insert(map, hana::make_pair(hana::second(pair), hana::first(pair)));
        });
    };

    auto m = hana::make_map(
        hana::make_pair(hana::type_c<int>, hana::int_c<1>),
        hana::make_pair(hana::type_c<float>, hana::int_c<2>),
        hana::make_pair(hana::int_c<3>, hana::type_c<void>)
    );

    BOOST_HANA_CONSTANT_CHECK(invert(m) ==
        hana::make_map(
            hana::make_pair(hana::int_c<1>, hana::type_c<int>),
            hana::make_pair(hana::int_c<2>, hana::type_c<float>),
            hana::make_pair(hana::type_c<void>, hana::int_c<3>)
        )
    );
}
