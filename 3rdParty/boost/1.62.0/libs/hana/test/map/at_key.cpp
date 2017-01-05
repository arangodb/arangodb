// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at_key.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/map.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>

#include <utility>
namespace hana = boost::hana;


template <int i>
auto key() { return hana::test::ct_eq<i>{}; }

template <int i>
auto val() { return hana::test::ct_eq<-i>{}; }

template <int i, int j>
auto p() { return ::minimal_product(key<i>(), val<j>()); }

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>()), key<0>()),
        val<0>()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>(), p<1,1>()), key<0>()),
        val<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>(), p<1,1>()), key<1>()),
        val<1>()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>(), p<1,1>(), p<2,2>()), key<0>()),
        val<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>(), p<1,1>(), p<2,2>()), key<1>()),
        val<1>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at_key(hana::make_map(p<0, 0>(), p<1,1>(), p<2,2>()), key<2>()),
        val<2>()
    ));

    // check operators
    auto m = hana::make_map(p<2, 2>(), p<1, 1>());
    auto const const_m = hana::make_map(p<2, 2>(), p<1, 1>());
    BOOST_HANA_CONSTANT_CHECK(hana::equal(m[key<1>()], val<1>()));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(const_m[key<1>()], val<1>()));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(std::move(m)[key<1>()], val<1>()));
}
