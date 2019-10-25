// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_INSERT_HPP
#define BOOST_HANA_TEST_AUTO_INSERT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/insert.hpp>
#include <boost/hana/integral_constant.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_insert{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto z = ct_eq<999>{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>, z),
        MAKE_TUPLE(z, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>, z),
        MAKE_TUPLE(ct_eq<0>{}, z)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<0>, z),
        MAKE_TUPLE(z, ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>, z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<2>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, z)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<0>, z),
        MAKE_TUPLE(z, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<1>, z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<2>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, z, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<3>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, z)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<0>, z),
        MAKE_TUPLE(z, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<1>, z),
        MAKE_TUPLE(ct_eq<0>{}, z, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<2>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, z, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<3>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, z, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::insert(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), hana::size_c<4>, z),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, z)
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_INSERT_HPP
