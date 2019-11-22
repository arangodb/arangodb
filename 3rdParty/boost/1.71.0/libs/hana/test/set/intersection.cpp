// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/intersection.hpp>
#include <boost/hana/set.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(),
            hana::make_set()
        ),
        hana::make_set()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}),
            hana::make_set()
        ),
        hana::make_set()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(),
            hana::make_set(ct_eq<0>{})
        ),
        hana::make_set()
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}),
            hana::make_set(ct_eq<1>{})
        ),
        hana::make_set()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}),
            hana::make_set(ct_eq<0>{})
        ),
        hana::make_set(ct_eq<0>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}, ct_eq<1>{}),
            hana::make_set(ct_eq<2>{}, ct_eq<3>{})
        ),
        hana::make_set()
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}, ct_eq<1>{}),
            hana::make_set(ct_eq<1>{}, ct_eq<2>{})
        ),
        hana::make_set(ct_eq<1>{})
    ));
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}, ct_eq<1>{}),
            hana::make_set(ct_eq<1>{}, ct_eq<0>{})
        ),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            hana::make_set(ct_eq<1>{}, ct_eq<0>{}, ct_eq<4>{})
        ),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{})
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::intersection(
            hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}),
            hana::make_set(ct_eq<1>{}, ct_eq<0>{}, ct_eq<3>{}, ct_eq<2>{}, ct_eq<4>{})
        ),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    ));
}
