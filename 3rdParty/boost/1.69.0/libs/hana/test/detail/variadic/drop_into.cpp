// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/drop_into.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
namespace vd = hana::detail::variadic;
using hana::test::ct_eq;


struct non_pod { virtual ~non_pod() { } };


int main() {
    hana::test::_injection<0> f{};

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<0>(f)(),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<0>(f)(ct_eq<0>{}),
        f(ct_eq<0>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<0>(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<0>{}, ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<0>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    ));


    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<1>(f)(ct_eq<0>{}),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<1>(f)(ct_eq<0>{}, ct_eq<1>{}),
        f(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<1>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<1>{}, ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<1>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));


    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<2>(f)(ct_eq<0>{}, ct_eq<1>{}),
        f()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<2>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}),
        f(ct_eq<2>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<2>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
        f(ct_eq<2>{}, ct_eq<3>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::drop_into<2>(f)(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}),
        f(ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    ));

    // make sure we can use non-pods
    vd::drop_into<1>(f)(ct_eq<0>{}, non_pod{});
    vd::drop_into<1>(f)(non_pod{}, ct_eq<1>{});
}
