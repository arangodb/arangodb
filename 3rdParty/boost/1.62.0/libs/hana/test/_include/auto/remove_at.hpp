// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_REMOVE_AT_HPP
#define BOOST_HANA_TEST_AUTO_REMOVE_AT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/remove_at.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_remove_at{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}),
                        hana::size_c<0>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                        hana::size_c<0>),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
                        hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                        hana::size_c<0>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                        hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
                        hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
                        hana::size_c<0>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
                        hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
                        hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
                        hana::size_c<3>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
                        hana::size_c<0>),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
                        hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
                        hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<3>{}, ct_eq<4>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
                        hana::size_c<3>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<4>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
                        hana::size_c<4>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));

    // remove_at_c
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<0>(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<0>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<1>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<0>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<1>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::remove_at_c<2>(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_REMOVE_AT_HPP
