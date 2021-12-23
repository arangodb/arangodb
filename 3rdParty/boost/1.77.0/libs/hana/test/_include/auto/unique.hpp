// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_UNIQUE_HPP
#define BOOST_HANA_TEST_AUTO_UNIQUE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/unique.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/equivalence_class.hpp>


TestCase test_unique{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE()),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(
            ct_eq<0>{}, ct_eq<0>{},
            ct_eq<1>{},
            ct_eq<2>{}, ct_eq<2>{}, ct_eq<2>{},
            ct_eq<3>{}, ct_eq<3>{}, ct_eq<3>{},
            ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<0>{})
    ));
}};

TestCase test_unique_by{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };

    auto a = [](auto z) { return ::equivalence_class(ct_eq<999>{}, z); };
    auto b = [](auto z) { return ::equivalence_class(ct_eq<888>{}, z); };
    auto c = [](auto z) { return ::equivalence_class(ct_eq<777>{}, z); };

    auto pred = [](auto x, auto y) {
        return hana::equal(x.unwrap, y.unwrap);
    };

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(), undefined{}),
        MAKE_TUPLE()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), c(ct_eq<0>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), c(ct_eq<1>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), c(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<0>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<0>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<1>{}), b(ct_eq<1>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique(MAKE_TUPLE(
            a(ct_eq<0>{}), b(ct_eq<0>{}),
            a(ct_eq<1>{}),
            a(ct_eq<2>{}), b(ct_eq<2>{}), c(ct_eq<2>{}),
            a(ct_eq<3>{}), b(ct_eq<3>{}), c(ct_eq<3>{}),
            a(ct_eq<0>{})), pred),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}), a(ct_eq<3>{}), a(ct_eq<0>{}))
    ));

    // unique.by
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique.by(pred, MAKE_TUPLE(
            a(ct_eq<0>{}), b(ct_eq<0>{}),
            a(ct_eq<1>{}),
            a(ct_eq<2>{}), b(ct_eq<2>{}), c(ct_eq<2>{}),
            a(ct_eq<3>{}), b(ct_eq<3>{}), c(ct_eq<3>{}),
            a(ct_eq<0>{}))),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}), a(ct_eq<3>{}), a(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unique.by(pred)(MAKE_TUPLE(
            a(ct_eq<0>{}), b(ct_eq<0>{}),
            a(ct_eq<1>{}),
            a(ct_eq<2>{}), b(ct_eq<2>{}), c(ct_eq<2>{}),
            a(ct_eq<3>{}), b(ct_eq<3>{}), c(ct_eq<3>{}),
            a(ct_eq<0>{}))),
        MAKE_TUPLE(a(ct_eq<0>{}), a(ct_eq<1>{}), a(ct_eq<2>{}), a(ct_eq<3>{}), a(ct_eq<0>{}))
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_UNIQUE_HPP
