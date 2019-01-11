// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_AP_HPP
#define BOOST_HANA_TEST_AUTO_AP_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/ap.hpp>
#include <boost/hana/equal.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_ap{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    hana::test::_injection<0> f{};
    hana::test::_injection<1> g{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f), MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(f(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f, g), MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f, g), MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), g(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f, g), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}), g(ct_eq<0>{}), g(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::ap(MAKE_TUPLE(f, g), MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(f(ct_eq<0>{}), f(ct_eq<1>{}), f(ct_eq<2>{}),
                   g(ct_eq<0>{}), g(ct_eq<1>{}), g(ct_eq<2>{}))
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_AP_HPP
