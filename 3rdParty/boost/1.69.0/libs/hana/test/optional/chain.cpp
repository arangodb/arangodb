// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto f = [](auto x) {
        return hana::just(hana::test::_injection<0>{}(x));
    };

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::chain(hana::just(ct_eq<3>{}), f),
        f(ct_eq<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::chain(hana::nothing, f),
        hana::nothing
    ));

    // test with operators
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::just(ct_eq<3>{}) | f,
        f(ct_eq<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::nothing | f,
        hana::nothing
    ));
}
