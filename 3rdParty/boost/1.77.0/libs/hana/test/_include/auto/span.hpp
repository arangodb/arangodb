// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_SPAN_HPP
#define BOOST_HANA_TEST_AUTO_SPAN_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/span.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/minimal_product.hpp>


TestCase test_span{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto z = ct_eq<999>{};
    auto pair = ::minimal_product;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(ct_eq<0>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(z), hana::equal.to(z)),
        pair(MAKE_TUPLE(z), MAKE_TUPLE())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(ct_eq<0>{}, z), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, z))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(z, ct_eq<0>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(z), MAKE_TUPLE(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(z, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(z), MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<2>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(z, z, ct_eq<2>{}), hana::equal.to(z)),
        pair(MAKE_TUPLE(z, z), MAKE_TUPLE(ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span(MAKE_TUPLE(z, z, z), hana::equal.to(z)),
        pair(MAKE_TUPLE(z, z, z), MAKE_TUPLE())
    ));

    // span.by
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span.by(hana::equal.to(z), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        hana::span(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(z))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::span.by(hana::equal.to(z))(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        hana::span(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::equal.to(z))
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_SPAN_HPP
