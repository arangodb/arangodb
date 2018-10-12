// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_NONE_OF_HPP
#define BOOST_HANA_TEST_AUTO_NONE_OF_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/none_of.hpp>
#include <boost/hana/not.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_none_of{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::none_of(
        MAKE_TUPLE(),
        [](auto) { return hana::false_c; }
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}),
        [](auto) { return hana::true_c; }
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}),
        [](auto) { return hana::false_c; }
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}),
        hana::equal.to(ct_eq<0>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}),
        hana::equal.to(ct_eq<999>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<0>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<1>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<2>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<3>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<4>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::none_of(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        hana::equal.to(ct_eq<999>{})
    ));

    // Make sure `none_of` short-circuits with runtime predicates
    // See http://stackoverflow.com/q/42012512/627587
    {
        {
            int counter = 0;
            auto tuple = MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{});
            hana::none_of(tuple, [&](auto) { ++counter; return true; });
            BOOST_HANA_RUNTIME_CHECK(counter == 1);
        }
        {
            int counter = 0;
            auto tuple = MAKE_TUPLE(ct_eq<999>{}, ct_eq<0>{}, ct_eq<999>{});
            hana::none_of(tuple, [&](auto x) -> bool {
                ++counter;
                return hana::equal(x, ct_eq<0>{});
            });
            BOOST_HANA_RUNTIME_CHECK(counter == 2);
        }
    }
}};

#endif // !BOOST_HANA_TEST_AUTO_NONE_OF_HPP
