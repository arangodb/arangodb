// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/members.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct MoveOnly {
    MoveOnly()                           = default;
    MoveOnly(MoveOnly&&)                 = default;
    MoveOnly(MoveOnly const&)            = delete;
    MoveOnly& operator=(MoveOnly&&)      = default;
    MoveOnly& operator=(MoveOnly const&) = delete;
};

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::members(obj()),
        ::seq()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::members(obj(ct_eq<0>{})),
        ::seq(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::members(obj(ct_eq<0>{}, ct_eq<1>{})),
        ::seq(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::members(obj(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})),
        ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));

    // make sure it works with move only types
    auto z1 = hana::members(obj(MoveOnly{}));
    auto z2 = hana::members(obj(MoveOnly{}, MoveOnly{}));
    (void)z1;
    (void)z2;
}
