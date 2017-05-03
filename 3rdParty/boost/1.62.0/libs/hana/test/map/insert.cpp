// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/map.hpp>

#include <laws/base.hpp>
#include <support/minimal_product.hpp>
namespace hana = boost::hana;


template <int i>
auto key() { return hana::test::ct_eq<i>{}; }

template <int i>
auto val() { return hana::test::ct_eq<-i>{}; }

template <int i, int j>
auto p() { return ::minimal_product(key<i>(), val<j>()); }

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(), p<1, 1>()),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>()), p<1, 99>()),
        hana::make_map(p<1, 1>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>()), p<2, 2>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>()), p<1, 99>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>()), p<2, 99>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>()), p<3, 3>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>()), p<1, 99>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>()), p<2, 99>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>()), p<3, 99>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>()), p<4, 4>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>(), p<4, 4>())
    ));
}
