// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_AT_HPP
#define BOOST_HANA_TEST_AUTO_AT_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>
#include <support/tracked.hpp>


namespace _test_at_detail { template <int> struct invalid { }; }


TestCase test_at{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;
    using _test_at_detail::invalid;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(ct_eq<0>{}), hana::size_c<0>),
        ct_eq<0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(ct_eq<0>{}, invalid<1>{}), hana::size_c<0>),
        ct_eq<0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(invalid<0>{}, ct_eq<1>{}), hana::size_c<1>),
        ct_eq<1>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(invalid<0>{}, ct_eq<1>{}, invalid<2>{}), hana::size_c<1>),
        ct_eq<1>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(invalid<0>{}, invalid<1>{}, ct_eq<2>{}), hana::size_c<2>),
        ct_eq<2>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::at(MAKE_TUPLE(invalid<0>{}, invalid<1>{}, ct_eq<2>{}, invalid<3>{}), hana::size_c<2>),
        ct_eq<2>{}
    ));

#ifndef MAKE_TUPLE_NO_CONSTEXPR
    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1), hana::size_c<0>),
        1
    ), "");
    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1, '2'), hana::size_c<0>),
        1
    ), "");
    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1, '2', 3.3), hana::size_c<0>),
        1
    ), "");

    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1, '2'), hana::size_c<1>),
        '2'
    ), "");
    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1, '2', 3.3), hana::size_c<1>),
        '2'
    ), "");

    static_assert(hana::equal(
        hana::at(MAKE_TUPLE(1, '2', 3.3), hana::size_c<2>),
        3.3
    ), "");
#endif

    // make sure we can use non-pods on both sides
    {
        // store the result to make sure `at` is executed.
        auto result = hana::at(MAKE_TUPLE(Tracked{0}, ct_eq<1>{}, Tracked{1}), hana::size_c<1>);;
        BOOST_HANA_CONSTANT_CHECK(hana::equal(result, ct_eq<1>{}));
    }

}};

#endif // !BOOST_HANA_TEST_AUTO_AT_HPP
