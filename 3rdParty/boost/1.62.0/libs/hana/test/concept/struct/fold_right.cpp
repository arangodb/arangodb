// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fold_right.hpp>
#include <boost/hana/integral_constant.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
#include <support/minimal_product.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


template <int i = 0>
struct undefined { };

struct MoveOnly {
    MoveOnly()                           = default;
    MoveOnly(MoveOnly&&)                 = default;
    MoveOnly(MoveOnly const&)            = delete;
    MoveOnly& operator=(MoveOnly&&)      = default;
    MoveOnly& operator=(MoveOnly const&) = delete;
};

int main() {
    constexpr auto pair = ::minimal_product;
    ct_eq<999> s{};
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(obj(), s, undefined<>{}),
        s
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(obj(ct_eq<0>{}), s, f),
        f(pair(hana::int_c<0>, ct_eq<0>{}), s)
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(obj(ct_eq<0>{}, ct_eq<1>{}), s, f),
        f(pair(hana::int_c<0>, ct_eq<0>{}), f(pair(hana::int_c<1>, ct_eq<1>{}), s))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::fold_right(obj(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), s, f),
        f(pair(hana::int_c<0>, ct_eq<0>{}),
            f(pair(hana::int_c<1>, ct_eq<1>{}),
                f(pair(hana::int_c<2>, ct_eq<2>{}), s)))
    ));

    // fold_right with move-only members
    hana::fold_right(obj(MoveOnly{}), 0, [](auto, int) { return 0; });
    hana::fold_right(obj(MoveOnly{}, MoveOnly{}), 0, [](auto, int) { return 0; });
}
