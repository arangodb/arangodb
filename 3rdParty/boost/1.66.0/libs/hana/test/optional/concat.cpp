// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concat.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto rv_nothing = [] { return hana::nothing; }; // rvalue nothing

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(rv_nothing(), hana::nothing),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::nothing, rv_nothing()),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(rv_nothing(), rv_nothing()),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(rv_nothing(), hana::just(ct_eq<0>{})),
        hana::just(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::just(ct_eq<0>{}), rv_nothing()),
        hana::just(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::nothing, hana::nothing),
        hana::nothing
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::nothing, hana::just(ct_eq<0>{})),
        hana::just(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::just(ct_eq<0>{}), hana::nothing),
        hana::just(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::concat(hana::just(ct_eq<0>{}), hana::just(ct_eq<1>{})),
        hana::just(ct_eq<0>{})
    ));
}
