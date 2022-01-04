// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/capture.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};

    // 0-arg capture
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture()(f)(),
        f()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture()(f)(ct_eq<0>{}),
        f(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture()(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture()(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    // 1-arg capture
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{})(f)(),
        f(ct_eq<77>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{})(f)(ct_eq<0>{}),
        f(ct_eq<77>{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{})(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<77>{}, ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{})(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<77>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    // 2-args capture
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{})(f)(),
        f(ct_eq<77>{}, ct_eq<88>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{})(f)(ct_eq<0>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{})(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{})(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    // 3-args capture
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{})(f)(),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{})(f)(ct_eq<0>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{}, ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{})(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{}, ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::capture(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{})(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<77>{}, ct_eq<88>{}, ct_eq<99>{}, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));
}
