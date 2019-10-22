// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_SORT_HPP
#define BOOST_HANA_TEST_AUTO_SORT_HPP

#include <boost/hana/all_of.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/permutations.hpp>
#include <boost/hana/sort.hpp>
#include <boost/hana/transform.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/equivalence_class.hpp>


TestCase test_sort{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    using hana::test::ct_ord;

    // Test without a custom predicate
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE()),
            MAKE_TUPLE()
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(ct_ord<0>{})),
            MAKE_TUPLE(ct_ord<0>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{})),
            MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(ct_ord<1>{}, ct_ord<0>{})),
            MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(ct_ord<1>{}, ct_ord<0>{}, ct_ord<4>{}, ct_ord<2>{})),
            MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, ct_ord<4>{})
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(ct_ord<1>{}, ct_ord<0>{}, ct_ord<-4>{}, ct_ord<2>{})),
            MAKE_TUPLE(ct_ord<-4>{}, ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{})
        ));
    }

    // Test with a custom predicate
    {
        auto pred = [](auto x, auto y) {
            return hana::less(x.unwrap, y.unwrap);
        };
        auto a = [](auto z) { return ::equivalence_class(ct_eq<999>{}, z); };
        auto b = [](auto z) { return ::equivalence_class(ct_eq<888>{}, z); };

        auto check = [=](auto ...sorted) {
            auto perms = hana::transform(
                hana::permutations(MAKE_TUPLE(a(sorted)...)),
                hana::sort.by(pred)
            );
            BOOST_HANA_CONSTANT_CHECK(hana::all_of(perms, [=](auto xs) {
                return hana::equal(xs, MAKE_TUPLE(a(sorted)...));
            }));
        };

        check();
        check(ct_ord<1>{});
        check(ct_ord<1>{}, ct_ord<2>{});
        check(ct_ord<1>{}, ct_ord<2>{}, ct_ord<3>{});

        // check stability
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{})), pred),
            MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(b(ct_ord<1>{}), a(ct_ord<1>{})), pred),
            MAKE_TUPLE(b(ct_ord<1>{}), a(ct_ord<1>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{})), pred),
            MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(a(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<1>{}), b(ct_ord<2>{})), pred),
            MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(b(ct_ord<1>{}), a(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{})), pred),
            MAKE_TUPLE(b(ct_ord<1>{}), a(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{}))
        ));
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(a(ct_ord<2>{}), b(ct_ord<1>{}), b(ct_ord<2>{}), a(ct_ord<1>{})), pred),
            MAKE_TUPLE(b(ct_ord<1>{}), a(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<2>{}))
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::sort(MAKE_TUPLE(a(ct_ord<1>{}), a(ct_ord<3>{}), b(ct_ord<1>{}), a(ct_ord<2>{}), b(ct_ord<3>{})), pred),
            MAKE_TUPLE(a(ct_ord<1>{}), b(ct_ord<1>{}), a(ct_ord<2>{}), a(ct_ord<3>{}), b(ct_ord<3>{}))
        ));
    }
}};

#endif // !BOOST_HANA_TEST_AUTO_SORT_HPP
