// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/is_subset.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/not.hpp>

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
    BOOST_HANA_CONSTANT_CHECK(hana::is_subset(
        hana::make_map(),
        hana::make_map()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::is_subset(
        hana::make_map(),
        hana::make_map(p<1, 1>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::is_subset(
        hana::make_map(),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::is_subset(
        hana::make_map(p<1, 1>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::is_subset(
        hana::make_map(p<2, 2>(), p<1, 1>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_subset(
        hana::make_map(p<3, 3>(), p<1, 1>()),
        hana::make_map(p<1, 1>(), p<2, 2>())
    )));
}
