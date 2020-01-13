// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/intersection.hpp>
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
        hana::intersection(
            hana::make_map(),
            hana::make_map()
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(p<1, 1>()),
            hana::make_map()
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(),
            hana::make_map(p<1, 1>())
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(p<1, 1>()),
            hana::make_map(p<1, 2>())
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(
                        p<1, 2>(),
                        p<2, 3>()),
            hana::make_map(
                        p<1, 3>(),
                        p<2, 4>(),
                        p<3, 5>())
        ),
        hana::make_map(
                    p<1, 2>(),
                    p<2, 3>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(
                        p<1, 3>(),
                        p<2, 4>(),
                        p<3, 5>()),
            hana::make_map(
                        p<1, 2>(),
                        p<2, 3>())
        ),
        hana::make_map(
                    p<1, 3>(),
                    p<2, 4>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_map(
                        p<1, 100>(),
                        p<2, 200>()),
            hana::make_map(
                        p<3, 300>(),
                        p<4, 400>())
        ),
        hana::make_map()
    ));

}
