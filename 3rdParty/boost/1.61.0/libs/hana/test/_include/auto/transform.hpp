// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_TRANSFORM_HPP
#define BOOST_HANA_TEST_AUTO_TRANSFORM_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/transform.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_transform{[] {
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    struct undefined { };
    constexpr hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(MAKE_TUPLE(), undefined{}),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(MAKE_TUPLE(ct_eq<1>{}), f),
        MAKE_TUPLE(f(ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}), f),
        MAKE_TUPLE(f(ct_eq<1>{}), f(ct_eq<2>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), f),
        MAKE_TUPLE(f(ct_eq<1>{}), f(ct_eq<2>{}), f(ct_eq<3>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::transform(MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}), f),
        MAKE_TUPLE(f(ct_eq<1>{}), f(ct_eq<2>{}), f(ct_eq<3>{}), f(ct_eq<4>{}))
    ));

#ifndef MAKE_TUPLE_NO_CONSTEXPR
    struct incr {
        constexpr int operator()(int i) const { return i + 1; }
    };

    static_assert(hana::equal(
        hana::transform(MAKE_TUPLE(1), incr{}),
        MAKE_TUPLE(2)
    ), "");

    static_assert(hana::equal(
        hana::transform(MAKE_TUPLE(1, 2), incr{}),
        MAKE_TUPLE(2, 3)
    ), "");

    static_assert(hana::equal(
        hana::transform(MAKE_TUPLE(1, 2, 3), incr{}),
        MAKE_TUPLE(2, 3, 4)
    ), "");

    static_assert(hana::equal(
        hana::transform(MAKE_TUPLE(1, 2, 3, 4), incr{}),
        MAKE_TUPLE(2, 3, 4, 5)
    ), "");
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_TRANSFORM_HPP
