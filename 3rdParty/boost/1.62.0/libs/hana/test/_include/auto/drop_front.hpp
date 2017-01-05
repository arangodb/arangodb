// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_DROP_FRONT_HPP
#define BOOST_HANA_TEST_AUTO_DROP_FRONT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/drop_front.hpp>
#include <boost/hana/drop_front_exactly.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_drop_front{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(), hana::size_c<0>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(), hana::size_c<1>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    // make sure hana::drop_front(xs) == hana::drop_front(xs, size_c<1>)
    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE()),
        hana::drop_front(MAKE_TUPLE(), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{})),
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{})),
        hana::drop_front(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}), hana::size_c<1>)
    ));
}};

TestCase test_drop_front_exactly{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(), hana::size_c<0>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<0>),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>),
        MAKE_TUPLE(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<2>),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::drop_front_exactly(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{},
                 ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}), hana::size_c<4>),
        MAKE_TUPLE(ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{})
    ));

    // make sure drop_front_exactly(xs) == drop_front_exactly(xs, size_c<1>)
    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{})),
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}), hana::size_c<1>)
    ));

    BOOST_HANA_CHECK(hana::equal(
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{})),
        hana::drop_front_exactly(MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}), hana::size_c<1>)
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_DROP_FRONT_HPP
