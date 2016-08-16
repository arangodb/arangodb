// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_TAKE_WHILE_HPP
#define BOOST_HANA_TEST_AUTO_TAKE_WHILE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not_equal.hpp>
#include <boost/hana/take_while.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_take_while{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto z = ct_eq<999>{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(), hana::not_equal.to(z)),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(z), hana::not_equal.to(z)),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}, z), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(z, ct_eq<2>{}), hana::not_equal.to(z)),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, z), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(ct_eq<1>{}, z, ct_eq<3>{}), hana::not_equal.to(z)),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::take_while(MAKE_TUPLE(z, ct_eq<2>{}, ct_eq<3>{}), hana::not_equal.to(z)),
        MAKE_TUPLE()
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_TAKE_WHILE_HPP
