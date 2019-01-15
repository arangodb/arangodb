// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_INSERT_RANGE_HPP
#define BOOST_HANA_TEST_AUTO_INSERT_RANGE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/insert_range.hpp>
#include <boost/hana/integral_constant.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/seq.hpp>


TestCase test_insert_range{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto foldable = ::seq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}),
            hana::size_c<0>,
            foldable()),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}),
            hana::size_c<0>,
            foldable(ct_eq<-1>{})),
        MAKE_TUPLE(ct_eq<-1>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}),
            hana::size_c<0>,
            foldable(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}),
            hana::size_c<0>,
            foldable(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<-3>{})),
        MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<-3>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<0>,
            foldable()),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<0>,
            foldable(ct_eq<-1>{})),
        MAKE_TUPLE(ct_eq<-1>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<0>,
            foldable(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<1>,
            foldable()),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<1>,
            foldable(ct_eq<-1>{})),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<-1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}),
            hana::size_c<1>,
            foldable(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<2>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert_range(
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
            hana::size_c<2>,
            foldable(ct_eq<-1>{}, ct_eq<-2>{})),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<-1>{}, ct_eq<-2>{}, ct_eq<3>{}, ct_eq<4>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_INSERT_RANGE_HPP
