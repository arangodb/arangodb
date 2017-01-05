// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/optional.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };

int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(f)(),
        hana::just(f())
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(f)(ct_eq<0>{}),
        hana::just(f(ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(f)(ct_eq<0>{}, ct_eq<1>{}),
        hana::just(f(ct_eq<0>{}, ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(undefined{})(),
        hana::nothing
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(undefined{})(ct_eq<0>{}),
        hana::nothing
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::sfinae(undefined{})(ct_eq<0>{}, ct_eq<1>{}),
        hana::nothing
    ));

    auto incr = hana::sfinae([](auto x) -> decltype(x + 1) {
        return x + 1;
    });

    BOOST_HANA_RUNTIME_CHECK(hana::equal(
        incr(1),
        hana::just(2)
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        incr(undefined{}),
        hana::nothing
    ));
    BOOST_HANA_RUNTIME_CHECK(hana::equal(
        hana::chain(hana::just(1), incr),
        hana::just(2)
    ));

    // using `sfinae` with a non-pod argument used to fail
    hana::sfinae(undefined{})(hana::test::Tracked{1});
    hana::sfinae([t = hana::test::Tracked{1}](auto) { return 1; })(hana::test::Tracked{1});
}
