// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/difference.hpp>
#include <boost/hana/equal.hpp>
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
        hana::difference(
            hana::make_map(),
            hana::make_map()
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>()),
            hana::make_map()
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>()),
            hana::make_map(p<2, 2>())
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>()),
            hana::make_map(p<1, 2>())
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>(),
                           p<2, 2>(),
                           p<3, 3>()),
            hana::make_map(p<2, 2>(),
                           p<4, 4>())
        ),
        hana::make_map(p<1, 1>(),
                       p<3, 3>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>(),
                           p<2, 2>()),
            hana::make_map(p<2, 3>(),
                           p<1, 4>())
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::difference(
            hana::make_map(p<1, 1>(),
                           p<2, 2>(),
                           p<3, 3>(),
                           p<4, 4>(),
                           p<5, 5>(),
                           p<6, 6>(),
                           p<7, 7>(),
                           p<8, 8>(),
                           p<9, 9>(),
                           p<10, 10>()),
            hana::make_map(p<0, 2>(),
                           p<2, 4>(),
                           p<3, 6>(),
                           p<4, 8>(),
                           p<5, 10>(),
                           p<20, 30>())
        ),
        hana::make_map(p<1, 1>(),
                       p<6, 6>(),
                       p<7, 7>(),
                       p<8, 8>(),
                       p<9, 9>(),
                       p<10, 10>())
    ));

}
