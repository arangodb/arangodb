// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/concept/struct.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include "minimal_struct.hpp"
#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto eq0 = hana::make_tuple(obj());
    auto eq1 = hana::make_tuple(
        obj(ct_eq<0>{}), obj(ct_eq<1>{}), obj(ct_eq<2>{})
    );
    auto eq2 = hana::make_tuple(
        obj(ct_eq<0>{}, ct_eq<0>{}),
        obj(ct_eq<0>{}, ct_eq<1>{}),
        obj(ct_eq<1>{}, ct_eq<0>{}),
        obj(ct_eq<1>{}, ct_eq<1>{}),
        obj(ct_eq<0>{}, ct_eq<2>{}),
        obj(ct_eq<2>{}, ct_eq<3>{})
    );

    hana::test::TestComparable<minimal_struct_tag<0>>{eq0};
    hana::test::TestComparable<minimal_struct_tag<1>>{eq1};
    hana::test::TestComparable<minimal_struct_tag<2>>{eq2};

    hana::test::TestFoldable<minimal_struct_tag<0>>{eq0};
    hana::test::TestFoldable<minimal_struct_tag<1>>{eq1};
    hana::test::TestFoldable<minimal_struct_tag<2>>{eq2};

    hana::test::TestSearchable<minimal_struct_tag<0>>{eq0, hana::make_tuple()};
    hana::test::TestSearchable<minimal_struct_tag<1>>{eq1, hana::make_tuple(hana::int_c<0>)};
    hana::test::TestSearchable<minimal_struct_tag<2>>{eq2, hana::make_tuple(hana::int_c<0>, hana::int_c<1>)};
}
