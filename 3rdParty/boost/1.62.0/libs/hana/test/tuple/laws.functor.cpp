// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>
#include <laws/monad_plus.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


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

    auto eq_values = hana::make_tuple(
        ct_eq<0>{},
        ct_eq<2>{},
        ct_eq<4>{}
    );

    auto predicates = hana::make_tuple(
        hana::equal.to(ct_eq<0>{}),
        hana::equal.to(ct_eq<2>{}),
        hana::equal.to(ct_eq<4>{}),
        hana::always(hana::true_c),
        hana::always(hana::false_c)
    );

    auto nested_eqs = hana::make_tuple(
          hana::make_tuple()
        , hana::make_tuple(
            hana::make_tuple(ct_eq<0>{})
        )
        , hana::make_tuple(
            hana::make_tuple(ct_eq<0>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{})
        )
        , hana::make_tuple(
            hana::make_tuple(ct_eq<0>{}),
            hana::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            hana::make_tuple(ct_eq<3>{}, ct_eq<4>{})
        )
    );

    hana::test::TestFunctor<hana::tuple_tag>{eq_tuples, eq_values};
    hana::test::TestApplicative<hana::tuple_tag>{eq_tuples};
    hana::test::TestMonad<hana::tuple_tag>{eq_tuples, nested_eqs};
    hana::test::TestMonadPlus<hana::tuple_tag>{eq_tuples, predicates, eq_values};
}
