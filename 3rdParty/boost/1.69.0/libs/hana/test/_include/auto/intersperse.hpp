// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_INTERSPERSE_HPP
#define BOOST_HANA_TEST_AUTO_INTERSPERSE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/intersperse.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_intersperse{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };

    auto z = ct_eq<999>{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(), undefined{}),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}), undefined{}),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, z, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, z, ct_eq<2>{}, z, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, z, ct_eq<2>{}, z, ct_eq<3>{}, z, ct_eq<4>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersperse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}), z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, z, ct_eq<2>{}, z, ct_eq<3>{}, z, ct_eq<4>{}, z, ct_eq<5>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_INTERSPERSE_HPP
