// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/iterable.hpp>
#include <laws/orderable.hpp>
#include <laws/sequence.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;
using hana::test::ct_ord;


int main() {
    auto eq_tuples = hana::make_tuple(
          hana::make_tuple()
        , hana::make_tuple(ct_eq<0>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
        , hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{})
    );

    auto ord_tuples = hana::make_tuple(
          hana::make_tuple()
        , hana::make_tuple(ct_ord<0>{})
        , hana::make_tuple(ct_ord<0>{}, ct_ord<1>{})
        , hana::make_tuple(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{})
        , hana::make_tuple(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, ct_ord<3>{})
        , hana::make_tuple(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, ct_ord<3>{}, ct_ord<4>{})
    );

    hana::test::TestComparable<hana::tuple_tag>{eq_tuples};
    hana::test::TestOrderable<hana::tuple_tag>{ord_tuples};
    hana::test::TestFoldable<hana::tuple_tag>{eq_tuples};
    hana::test::TestIterable<hana::tuple_tag>{eq_tuples};
    hana::test::TestSequence<hana::tuple_tag>{};
}
