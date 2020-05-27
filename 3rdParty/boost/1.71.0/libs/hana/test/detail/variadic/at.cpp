// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/detail/variadic/at.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
namespace vd = hana::detail::variadic;
using hana::test::ct_eq;


struct non_pod { virtual ~non_pod() { } };

template <int i> struct y { };

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<0>(ct_eq<0>{}),
        ct_eq<0>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<0>(ct_eq<0>{}, ct_eq<1>{}),
        ct_eq<0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<1>(y<0>{}, ct_eq<1>{}),
        ct_eq<1>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<0>(ct_eq<0>{}, y<1>{}, y<2>{}),
        ct_eq<0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<1>(y<0>{}, ct_eq<1>{}, y<2>{}),
        ct_eq<1>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<2>(y<0>{}, y<1>{}, ct_eq<2>{}),
        ct_eq<2>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<0>(ct_eq<0>{}, y<1>{}, y<2>{}, y<3>{}),
        ct_eq<0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<1>(y<0>{}, ct_eq<1>{}, y<2>{}, y<3>{}),
        ct_eq<1>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<2>(y<0>{}, y<1>{}, ct_eq<2>{}, y<3>{}),
        ct_eq<2>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<3>(y<0>{}, y<1>{}, y<2>{}, ct_eq<3>{}),
        ct_eq<3>{}
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<0>(ct_eq<0>{}, y<1>{}, y<2>{}, y<3>{}, y<4>{}),
        ct_eq<0>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<1>(y<0>{}, ct_eq<1>{}, y<2>{}, y<3>{}, y<4>{}),
        ct_eq<1>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<2>(y<0>{}, y<1>{}, ct_eq<2>{}, y<3>{}, y<4>{}),
        ct_eq<2>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<3>(y<0>{}, y<1>{}, y<2>{}, ct_eq<3>{}, y<4>{}),
        ct_eq<3>{}
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        vd::at<4>(y<0>{}, y<1>{}, y<2>{}, y<3>{}, ct_eq<4>{}),
        ct_eq<4>{}
    ));

    // make sure we can use non-pods on both side of the fetched object
    vd::at<0>(ct_eq<0>{}, non_pod{});
    vd::at<1>(non_pod{}, ct_eq<1>{});

    // make sure it works with const objects
    int const i = 1;
    vd::at<0>(i);
}
