// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_LEXICOGRAPHICAL_COMPARE_HPP
#define BOOST_HANA_TEST_AUTO_LEXICOGRAPHICAL_COMPARE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/lexicographical_compare.hpp>
#include <boost/hana/not.hpp>

#include <laws/base.hpp>
#include "test_case.hpp"


TestCase test_lexicographical_compare{[]{
    namespace hana = boost::hana;
    using hana::test::ct_ord;

    struct undefined { };

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(),
        MAKE_TUPLE()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(),
        MAKE_TUPLE(undefined{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(undefined{}),
        MAKE_TUPLE()
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}),
        MAKE_TUPLE(ct_ord<0>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}),
        MAKE_TUPLE(ct_ord<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<1>{}),
        MAKE_TUPLE(ct_ord<0>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, undefined{}),
        MAKE_TUPLE(ct_ord<0>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}),
        MAKE_TUPLE(ct_ord<0>{}, undefined{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<0>{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<0>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<3>{}, undefined{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<3>{}, undefined{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::lexicographical_compare(
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<3>{}),
        MAKE_TUPLE(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, undefined{})
    )));
}};

#endif // !BOOST_HANA_TEST_AUTO_LEXICOGRAPHICAL_COMPARE_HPP
