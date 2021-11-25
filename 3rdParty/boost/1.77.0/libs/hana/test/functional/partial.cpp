// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/partial.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f)(),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f)(ct_eq<1>{}),
        f(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f)(ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f)(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{})(),
        f(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{})(ct_eq<2>{}),
        f(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{})(ct_eq<2>{}, ct_eq<3>{}),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{}, ct_eq<2>{})(),
        f(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{}, ct_eq<2>{})(ct_eq<3>{}),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{}, ct_eq<2>{})(ct_eq<3>{}, ct_eq<4>{}),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::partial(f, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})(),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
}
