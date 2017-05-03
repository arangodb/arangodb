// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_GROUP_HPP
#define BOOST_HANA_TEST_AUTO_GROUP_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/group.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/equivalence_class.hpp>


TestCase test_group{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    struct undefined { };

    // Test without a custom predicate
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE()),
            MAKE_TUPLE()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}),
                MAKE_TUPLE(ct_eq<1>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<0>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<1>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}),
                MAKE_TUPLE(ct_eq<1>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<0>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}),
                MAKE_TUPLE(ct_eq<1>{}),
                MAKE_TUPLE(ct_eq<0>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{}, ct_eq<0>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<1>{}),
                MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<1>{})),
            MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}),
                 MAKE_TUPLE(ct_eq<1>{}, ct_eq<1>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<2>{}, ct_eq<2>{})),
            MAKE_TUPLE(
                MAKE_TUPLE(ct_eq<0>{}, ct_eq<0>{}),
                MAKE_TUPLE(ct_eq<1>{}, ct_eq<1>{}),
                MAKE_TUPLE(ct_eq<2>{}, ct_eq<2>{}, ct_eq<2>{}))
        ));
    }

    // Test with a custom predicate
    {
        auto a = [](auto z) { return ::equivalence_class(ct_eq<999>{}, z); };
        auto b = [](auto z) { return ::equivalence_class(ct_eq<888>{}, z); };

        auto pred = [](auto x, auto y) {
            return hana::equal(x.unwrap, y.unwrap);
        };

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(), undefined{}),
            MAKE_TUPLE()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(a(ct_eq<0>{})), pred),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{})))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})), pred),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), a(ct_eq<1>{})), pred),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})),
                MAKE_TUPLE(a(ct_eq<1>{})))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), a(ct_eq<1>{}), b(ct_eq<1>{})), pred),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})),
                MAKE_TUPLE(a(ct_eq<1>{}), b(ct_eq<1>{})))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), a(ct_eq<1>{}), b(ct_eq<1>{}), b(ct_eq<0>{})), pred),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})),
                MAKE_TUPLE(a(ct_eq<1>{}), b(ct_eq<1>{})),
                MAKE_TUPLE(b(ct_eq<0>{})))
        ));

        // Test group.by syntactic sugar
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group.by(pred, MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), a(ct_eq<1>{}), b(ct_eq<1>{}), b(ct_eq<0>{}))),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})),
                MAKE_TUPLE(a(ct_eq<1>{}), b(ct_eq<1>{})),
                MAKE_TUPLE(b(ct_eq<0>{})))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::group.by(pred)(MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{}), a(ct_eq<1>{}), b(ct_eq<1>{}), b(ct_eq<0>{}))),
            MAKE_TUPLE(
                MAKE_TUPLE(a(ct_eq<0>{}), b(ct_eq<0>{})),
                MAKE_TUPLE(a(ct_eq<1>{}), b(ct_eq<1>{})),
                MAKE_TUPLE(b(ct_eq<0>{})))
        ));
    }
}};

#endif // !BOOST_HANA_TEST_AUTO_GROUP_HPP
