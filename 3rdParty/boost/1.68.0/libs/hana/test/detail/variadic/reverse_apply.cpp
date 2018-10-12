// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/reverse_apply.hpp>
#include <boost/hana/detail/variadic/reverse_apply/flat.hpp>
#include <boost/hana/detail/variadic/reverse_apply/unrolled.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


auto check = [](auto reverse_apply) {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}),
        f(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        f(ct_eq<4>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}),
        f(ct_eq<5>{}, ct_eq<4>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}),
        f(ct_eq<6>{}, ct_eq<5>{}, ct_eq<4>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}),
        f(ct_eq<7>{}, ct_eq<6>{}, ct_eq<5>{}, ct_eq<4>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        reverse_apply(f, ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}),
        f(ct_eq<8>{}, ct_eq<7>{}, ct_eq<6>{}, ct_eq<5>{}, ct_eq<4>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<1>{}, ct_eq<0>{})
    ));
};

int main() {
    check(hana::detail::variadic::reverse_apply);
    check([](auto f, auto ...x) {
        return hana::detail::variadic::reverse_apply_flat(f, x...);
    });
    check([](auto f, auto ...x) {
        return hana::detail::variadic::reverse_apply_unrolled(f, x...);
    });
}
