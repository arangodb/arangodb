// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef BOOST_HANA_TEST_AUTO_MAKE_HPP
#define BOOST_HANA_TEST_AUTO_MAKE_HPP

#include <boost/hana/assert.hpp>
#include <boost/hana/core/make.hpp>
#include <boost/hana/equal.hpp>

#include "test_case.hpp"
#include <laws/base.hpp>


TestCase test_make{[]{
    namespace hana = boost::hana;
    using hana::test::ct_eq;

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        MAKE_TUPLE(),
        hana::make<TUPLE_TAG>()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        MAKE_TUPLE(ct_eq<0>{}),
        hana::make<TUPLE_TAG>(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}),
        hana::make<TUPLE_TAG>(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        MAKE_TUPLE(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        hana::make<TUPLE_TAG>(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
}};

#endif // !BOOST_HANA_TEST_AUTO_MAKE_HPP
