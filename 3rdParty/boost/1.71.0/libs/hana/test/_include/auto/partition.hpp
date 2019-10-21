// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_PARTITION_HPP
#define BOOST_HANA_TEST_AUTO_PARTITION_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/partition.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/minimal_product.hpp>


TestCase test_partition{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };

    auto pair = ::minimal_product;
    auto pred = hana::in ^ MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<-3>{}, ct_eq<-4>{}, ct_eq<-5>{});

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(), undefined{}),
        pair(MAKE_TUPLE(), MAKE_TUPLE())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<0>{}), pred),
        pair(MAKE_TUPLE(),
             MAKE_TUPLE(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), pred),
        pair(MAKE_TUPLE(),
             MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<-1>{}), pred),
        pair(MAKE_TUPLE(ct_eq<-1>{}),
             MAKE_TUPLE())
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<0>{}, ct_eq<2>{}), pred),
        pair(MAKE_TUPLE(ct_eq<-1>{}),
             MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<0>{}, ct_eq<-3>{}, ct_eq<2>{}, ct_eq<-5>{}, ct_eq<6>{}), pred),
        pair(MAKE_TUPLE(ct_eq<-3>{}, ct_eq<-5>{}),
             MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<6>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<2>{}, ct_eq<-3>{}, ct_eq<0>{}, ct_eq<-3>{}, ct_eq<4>{}), pred),
        pair(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-3>{}, ct_eq<-3>{}),
             MAKE_TUPLE(ct_eq<2>{}, ct_eq<0>{}, ct_eq<4>{}))
    ));

    // partition.by
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition.by(pred, MAKE_TUPLE(ct_eq<-1>{}, ct_eq<0>{}, ct_eq<2>{})),
        hana::partition(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<0>{}, ct_eq<2>{}), pred)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partition.by(pred)(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<0>{}, ct_eq<2>{})),
        hana::partition(MAKE_TUPLE(ct_eq<-1>{}, ct_eq<0>{}, ct_eq<2>{}), pred)
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_PARTITION_HPP
