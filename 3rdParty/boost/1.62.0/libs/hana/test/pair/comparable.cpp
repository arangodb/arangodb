// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    BOOST_HANA_CONSTANT_CHECK(
        hana::make_pair(ct_eq<1>{}, ct_eq<2>{}) ==
        hana::make_pair(ct_eq<1>{}, ct_eq<2>{})
    );

    BOOST_HANA_CONSTANT_CHECK(
        hana::make_pair(ct_eq<1>{}, ct_eq<3>{}) !=
        hana::make_pair(ct_eq<1>{}, ct_eq<2>{})
    );

    hana::test::TestComparable<hana::pair_tag>{hana::make_tuple(
        hana::make_pair(ct_eq<3>{}, ct_eq<3>{})
      , hana::make_pair(ct_eq<3>{}, ct_eq<4>{})
      , hana::make_pair(ct_eq<4>{}, ct_eq<3>{})
      , hana::make_pair(ct_eq<4>{}, ct_eq<4>{})
    )};
}
