// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/union.hpp>

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
        hana::union_(
            hana::make_map(),
            hana::make_map()
        ),
        hana::make_map()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::union_(
            hana::make_map(
                       p<1, 1>()
            ),
            hana::make_map()
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::union_(
            hana::make_map(),
            hana::make_map(p<1, 1>())
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::union_(
               hana::make_map(p<1, 1>()),
               hana::make_map(p<1, 1>())
        ),
        hana::make_map(p<1, 1>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::union_(
               hana::make_map(p<1, 2>()),
               hana::make_map(p<1, 3>())
        ),
        hana::make_map(p<1, 3>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::union_(
               hana::make_map(p<1, 10>(),
                              p<2, 20>(),
                              p<3, 30>()),
               hana::make_map(p<4, 40>(),
                              p<5, 50>(),
                              p<1, 100>())
        ),
        hana::make_map(p<2, 20>(),
                       p<3, 30>(),
                       p<4, 40>(),
                       p<5, 50>(),
                       p<1, 100>())
    ));
}
