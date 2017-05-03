// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_TAKE_BACK_HPP
#define BOOST_HANA_TEST_AUTO_TAKE_BACK_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/take_back.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_take_back{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(), hana::size_c<0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(), hana::size_c<1>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<3>),
        MAKE_TUPLE(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<3>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<3>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_back(MAKE_TUPLE(
            ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{},
            ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{}),
            hana::size_c<5>),
        MAKE_TUPLE(ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_TAKE_BACK_HPP
