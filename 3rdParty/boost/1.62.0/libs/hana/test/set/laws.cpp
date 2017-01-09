// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/set.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto eqs = hana::make_tuple(
        hana::make_set(),
        hana::make_set(ct_eq<0>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{}),
        hana::make_set(ct_eq<1>{}, ct_eq<0>{}),
        hana::make_set(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
    );

    auto keys = hana::make_tuple(
        ct_eq<2>{},
        ct_eq<3>{}
    );

    hana::test::TestComparable<hana::set_tag>{eqs};
    hana::test::TestSearchable<hana::set_tag>{eqs, keys};
    hana::test::TestFoldable<hana::set_tag>{eqs};
}
