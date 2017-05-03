// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        obj(),
        obj()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        obj(ct_eq<0>{}),
        obj(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        obj(ct_eq<0>{}),
        obj(ct_eq<1>{})
    )));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        obj(ct_eq<0>{}, ct_eq<1>{}),
        obj(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        obj(ct_eq<1>{}, ct_eq<0>{}),
        obj(ct_eq<0>{}, ct_eq<1>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        obj(ct_eq<0>{}, ct_eq<99>{}),
        obj(ct_eq<0>{}, ct_eq<1>{})
    )));
    BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(
        obj(ct_eq<99>{}, ct_eq<1>{}),
        obj(ct_eq<0>{}, ct_eq<1>{})
    )));
}
