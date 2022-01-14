// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/functor.hpp>
#include <laws/monad.hpp>
#include <laws/monad_plus.hpp>

#include <tuple>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto tuples = hana::make_tuple(
          std::make_tuple()
        , std::make_tuple(ct_eq<0>{})
        , std::make_tuple(ct_eq<0>{}, ct_eq<1>{})
        , std::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , std::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{}, ct_eq<4>{})
    );

    auto values = hana::make_tuple(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});

    auto nested_tuples = hana::make_tuple(
          std::make_tuple()
        , std::make_tuple(
            std::make_tuple(ct_eq<0>{}))
        , std::make_tuple(
            std::make_tuple(ct_eq<0>{}),
            std::make_tuple(ct_eq<1>{}, ct_eq<2>{}))
        , std::make_tuple(
            std::make_tuple(ct_eq<0>{}),
            std::make_tuple(ct_eq<1>{}, ct_eq<2>{}),
            std::make_tuple(ct_eq<3>{}, ct_eq<4>{}))
    );

    auto predicates = hana::make_tuple(
        hana::equal.to(ct_eq<0>{}), hana::equal.to(ct_eq<1>{}), hana::equal.to(ct_eq<2>{}),
        hana::always(hana::false_c), hana::always(hana::true_c)
    );

    hana::test::TestFunctor<hana::ext::std::tuple_tag>{tuples, values};
    hana::test::TestApplicative<hana::ext::std::tuple_tag>{tuples};
    hana::test::TestMonad<hana::ext::std::tuple_tag>{tuples, nested_tuples};
    hana::test::TestMonadPlus<hana::ext::std::tuple_tag>{tuples, predicates, values};
}
