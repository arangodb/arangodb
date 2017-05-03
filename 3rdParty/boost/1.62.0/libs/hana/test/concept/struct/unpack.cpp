// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/unpack.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
#include <support/minimal_product.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    constexpr auto pair = ::minimal_product;
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(obj(), f),
        f()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(obj(ct_eq<0>{}), f),
        f(pair(hana::int_c<0>, ct_eq<0>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(obj(ct_eq<0>{}, ct_eq<1>{}), f),
        f(pair(hana::int_c<0>, ct_eq<0>{}),
          pair(hana::int_c<1>, ct_eq<1>{}))
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::unpack(obj(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}), f),
        f(pair(hana::int_c<0>, ct_eq<0>{}),
          pair(hana::int_c<1>, ct_eq<1>{}),
          pair(hana::int_c<2>, ct_eq<2>{}))
    ));
}
