// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_PERMUTATIONS_HPP
#define BOOST_HANA_TEST_AUTO_PERMUTATIONS_HPP

#include <boost/hana/and.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/is_subset.hpp>
#include <boost/hana/permutations.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_permutations{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    auto is_permutation = [](auto xs, auto ys) {
        return hana::and_(hana::is_subset(xs, ys), hana::is_subset(ys, xs));
    };

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::permutations(MAKE_TUPLE()),
        MAKE_TUPLE(MAKE_TUPLE())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::permutations(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(MAKE_TUPLE(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(is_permutation(
        hana::permutations(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(is_permutation(
        hana::permutations(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<0>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
        )
    ));

    BOOST_HANA_CONSTANT_CHECK(is_permutation(
        hana::permutations(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})),
        MAKE_TUPLE(
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<3>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<3>{}, ct_eq<1>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<0>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{}, ct_eq<2>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{}, ct_eq<3>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<0>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<3>{}, ct_eq<0>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<1>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<0>{}, ct_eq<3>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{}, ct_eq<3>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<1>{}, ct_eq<3>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<3>{}, ct_eq<0>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<2>{}, ct_eq<3>{}, ct_eq<1>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<0>{}, ct_eq<2>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<1>{}, ct_eq<0>{}, ct_eq<2>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<0>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<2>{}, ct_eq<0>{}, ct_eq<1>{}),
            MAKE_TUPLE(ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
        )
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_PERMUTATIONS_HPP
