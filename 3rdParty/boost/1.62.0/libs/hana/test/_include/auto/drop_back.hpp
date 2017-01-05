// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_DROP_BACK_HPP
#define BOOST_HANA_TEST_AUTO_DROP_BACK_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_back.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_drop_back{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(), hana::size_c<0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(), hana::size_c<1>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<2>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), hana::size_c<3>),
        MAKE_TUPLE()
    ));

    // make sure hana::drop_back(xs) == hana::drop_back(xs, hana::size_c<1>)
    BOOST_HANA_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE()),
        hana::drop_back(MAKE_TUPLE(), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{})),
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{})),
        hana::drop_back(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}), hana::size_c<1>)
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_DROP_BACK_HPP
