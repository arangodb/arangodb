// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>
#include <laws/monad_plus.hpp>
#include <laws/orderable.hpp>
#include <laws/searchable.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;
using hana::test::ct_ord;


int main() {
    auto ords = hana::make_tuple(
        hana::nothing,
        hana::just(ct_ord<0>{}),
        hana::just(ct_ord<1>{}),
        hana::just(ct_ord<2>{})
    );

    auto eqs = hana::make_tuple(
        hana::nothing,
        hana::just(ct_eq<0>{}),
        hana::just(ct_eq<1>{}),
        hana::just(ct_eq<2>{})
    );

    auto eq_values = hana::make_tuple(ct_eq<0>{}, ct_eq<2>{}, ct_eq<3>{});

    auto predicates = hana::make_tuple(
        hana::equal.to(ct_eq<0>{}),
        hana::equal.to(ct_eq<2>{}),
        hana::equal.to(ct_eq<3>{}),
        hana::always(hana::false_c),
        hana::always(hana::true_c)
    );

    auto nested_eqs = hana::make_tuple(
        hana::nothing,
        hana::just(hana::just(ct_eq<0>{})),
        hana::just(hana::nothing),
        hana::just(hana::just(ct_eq<2>{}))
    );


    hana::test::TestComparable<hana::optional_tag>{eqs};
    hana::test::TestOrderable<hana::optional_tag>{ords};
    hana::test::TestFunctor<hana::optional_tag>{eqs, eq_values};
    hana::test::TestApplicative<hana::optional_tag>{eqs};
    hana::test::TestMonad<hana::optional_tag>{eqs, nested_eqs};
    hana::test::TestMonadPlus<hana::optional_tag>{eqs, predicates, eq_values};
    hana::test::TestSearchable<hana::optional_tag>{eqs, eq_values};
    hana::test::TestSearchable<hana::optional_tag>{
        hana::make_tuple(
            hana::just(hana::true_c),
            hana::just(hana::false_c),
            hana::nothing
        ),
        hana::make_tuple(hana::true_c, hana::false_c)
    };
    hana::test::TestFoldable<hana::optional_tag>{eqs};
}
