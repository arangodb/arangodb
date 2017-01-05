// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/erase_key.hpp>
#include <boost/hana/set.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(), undefined{}),
        hana::make_set()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}), ct_eq<0>{}),
        hana::make_set()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}), ct_eq<99>{}),
        hana::make_set(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}), ct_eq<0>{}),
        hana::make_set(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}), ct_eq<1>{}),
        hana::make_set(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}), ct_eq<99>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), ct_eq<0>{}),
        hana::make_set(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), ct_eq<1>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), ct_eq<2>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::erase_key(hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), ct_eq<99>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
}
