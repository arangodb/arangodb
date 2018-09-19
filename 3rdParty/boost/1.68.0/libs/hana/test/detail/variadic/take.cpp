// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/take.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
namespace vd = hana::detail::variadic;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};

    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<0>()(f),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<0>(ct_eq<1>{})(f),
            f()
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<0>(ct_eq<1>{}, ct_eq<2>{})(f),
            f()
        ));
    }
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<1>(ct_eq<1>{})(f),
            f(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<1>(ct_eq<1>{}, ct_eq<2>{})(f),
            f(ct_eq<1>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<1>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})(f),
            f(ct_eq<1>{})
        ));
    }
    {
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{})(f),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{})
        ));

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            vd::take<8>(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{}, ct_eq<9>{})(f),
            f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{}, ct_eq<6>{}, ct_eq<7>{}, ct_eq<8>{})
        ));
    }
}
