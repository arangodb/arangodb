// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/demux.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct MoveOnly {
    MoveOnly() = default;
    MoveOnly(MoveOnly const&) = delete;
    MoveOnly(MoveOnly&&) = default;
};

int main() {
    hana::test::_injection<0> f{};
    hana::test::_injection<1> g{};
    hana::test::_injection<2> h{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)()(),
        f()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g)(ct_eq<1>{}),
        f(g(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g)(ct_eq<1>{}, ct_eq<2>{}),
        f(g(ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(g(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g, h)(ct_eq<1>{}),
        f(g(ct_eq<1>{}), h(ct_eq<1>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g, h)(ct_eq<1>{}, ct_eq<2>{}),
        f(g(ct_eq<1>{}, ct_eq<2>{}), h(ct_eq<1>{}, ct_eq<2>{}))
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::demux(f)(g, h)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(g(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}), h(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}))
    ));

    // Make sure we can pass move-only types when a single function
    // is demux'ed. In other words, make sure the arguments are
    // perfect-forwarded when a single function is demux'ed.
    {
        hana::demux(f)(g)(MoveOnly{});
        hana::demux(f)(g)(MoveOnly{}, MoveOnly{});
        hana::demux(f)(g)(MoveOnly{}, MoveOnly{}, MoveOnly{});
    }
}
