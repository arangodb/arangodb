// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/any_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
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
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        hana::make_map(),
        hana::equal.to(key<1>()
    ))));

    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        hana::make_map(p<1, 1>()),
        hana::equal.to(key<1>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        hana::make_map(p<1, 1>()),
        hana::equal.to(key<2>())
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        hana::make_map(p<1, 1>(), p<2, 2>()),
        hana::equal.to(key<1>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::any_of(
        hana::make_map(p<1, 1>(), p<2, 2>()),
        hana::equal.to(key<2>())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::any_of(
        hana::make_map(p<1, 1>(), p<2, 2>()),
        hana::equal.to(key<3>())
    )));
}
