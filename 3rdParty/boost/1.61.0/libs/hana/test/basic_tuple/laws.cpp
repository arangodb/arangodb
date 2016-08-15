// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/basic_tuple.hpp>

#include <laws/base.hpp>
#include <laws/foldable.hpp>
#include <laws/functor.hpp>
#include <laws/iterable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto eq_tuples = hana::make_basic_tuple(
          hana::make_basic_tuple()
        , hana::make_basic_tuple(ct_eq<0>{})
        , hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{})
        , hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
        , hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
        , hana::make_basic_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{}, ct_eq<5>{})
    );

    auto eq_values = hana::make_basic_tuple(
        ct_eq<0>{},
        ct_eq<2>{},
        ct_eq<4>{}
    );

    hana::test::TestFunctor<hana::basic_tuple_tag>{eq_tuples, eq_values};
    hana::test::TestFoldable<hana::basic_tuple_tag>{eq_tuples};
    hana::test::TestIterable<hana::basic_tuple_tag>{eq_tuples};
}
