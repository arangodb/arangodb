// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
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
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(),
        key<0>()
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(),
        key<1>()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>()),
        key<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>()),
        key<1>()
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>()),
        key<2>()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>()),
        key<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>()),
        key<1>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>()),
        key<2>()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>()),
        key<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>()),
        key<1>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>()),
        key<2>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>()),
        key<3>()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<0>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<1>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<2>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<3>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<4>()
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<5>()
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<6>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<7>()
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<8>()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::contains(
        hana::make_map(p<0,0>(), p<1,1>(), p<2,2>(), p<3,3>(), p<6,6>(), p<8,8>()),
        key<9>()
    )));
}
