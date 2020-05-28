// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/lockstep.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};
    hana::test::_injection<1> g{};
    hana::test::_injection<2> h{};
    hana::test::_injection<3> i{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::lockstep(f)()(),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::lockstep(f)(g)(ct_eq<1>{}),
        f(g(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::lockstep(f)(g, h)(ct_eq<1>{}, ct_eq<2>{}),
        f(g(ct_eq<1>{}), h(ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::lockstep(f)(g, h, i)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(g(ct_eq<1>{}), h(ct_eq<2>{}), i(ct_eq<3>{}))
    ));
}
