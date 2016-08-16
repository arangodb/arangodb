// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_REVERSE_HPP
#define BOOST_HANA_TEST_AUTO_REVERSE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/reverse.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_reverse{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    using hana::test::cx_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::reverse(MAKE_TUPLE()),
        MAKE_TUPLE()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::reverse(MAKE_TUPLE(ct_eq<0>{})),
        MAKE_TUPLE(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::reverse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{})),
        MAKE_TUPLE(ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::reverse(MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        MAKE_TUPLE(ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));


#ifndef MAKE_TUPLE_NO_CONSTEXPR
    static_assert(hana::equal(
        hana::reverse(MAKE_TUPLE(cx_eq<1>{})),
        MAKE_TUPLE(cx_eq<1>{})
    ), "");
    static_assert(hana::equal(
        hana::reverse(MAKE_TUPLE(cx_eq<1>{}, cx_eq<2>{})),
        MAKE_TUPLE(cx_eq<2>{}, cx_eq<1>{})
    ), "");
    static_assert(hana::equal(
        hana::reverse(MAKE_TUPLE(cx_eq<1>{}, cx_eq<2>{}, cx_eq<3>{})),
        MAKE_TUPLE(cx_eq<3>{}, cx_eq<2>{}, cx_eq<1>{})
    ), "");
#endif
}};

#endif // !BOOST_HANA_TEST_AUTO_REVERSE_HPP
