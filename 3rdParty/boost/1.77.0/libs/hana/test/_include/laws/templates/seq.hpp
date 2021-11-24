// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/applicative.hpp>
#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/functor.hpp>
#include <laws/iterable.hpp>
#include <laws/monad.hpp>
#include <laws/monad_plus.hpp>
#include <laws/orderable.hpp>
#include <laws/searchable.hpp>
#include <laws/sequence.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;
using hana::test::ct_ord;


int main() {
    auto eqs = hana::make_tuple(
          ::seq()
        , ::seq(ct_eq<0>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    );
    (void)eqs;

    auto nested_eqs = hana::make_tuple(
          ::seq()
        , ::seq(
            ::seq(ct_eq<0>{}))
        , ::seq(
            ::seq(ct_eq<0>{}),
            ::seq(ct_eq<1>{}, ct_eq<2>{}))
        , ::seq(
            ::seq(ct_eq<0>{}),
            ::seq(ct_eq<1>{}, ct_eq<2>{}),
            ::seq(ct_eq<3>{}, ct_eq<4>{}))
    );
    (void)nested_eqs;

    auto eq_keys = hana::make_tuple(ct_eq<0>{}, ct_eq<3>{}, ct_eq<10>{});
    (void)eq_keys;

    auto predicates = hana::make_tuple(
        hana::equal.to(ct_eq<0>{}), hana::equal.to(ct_eq<3>{}), hana::equal.to(ct_eq<10>{}),
        hana::always(hana::true_c), hana::always(hana::false_c)
    );
    (void)predicates;

    auto ords = hana::make_tuple(
          ::seq()
        , ::seq(ct_ord<0>{})
        , ::seq(ct_ord<0>{}, ct_ord<1>{})
        , ::seq(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{})
        , ::seq(ct_ord<0>{}, ct_ord<1>{}, ct_ord<2>{}, ct_ord<3>{})
    );
    (void)ords;

    //////////////////////////////////////////////////////////////////////////
    // Comparable, Orderable
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_ORDERABLE
    hana::test::TestComparable<::Seq>{eqs};
    hana::test::TestOrderable<::Seq>{ords};
#endif

#ifdef BOOST_HANA_TEST_ITERABLE
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    hana::test::TestFoldable<::Seq>{eqs};

    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    {
        hana::test::TestIterable<::Seq>{eqs};
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_SEARCHABLE
    {
        hana::test::TestSearchable<::Seq>{eqs, eq_keys};

        auto bools = hana::make_tuple(
              ::seq(hana::true_c)
            , ::seq(hana::false_c)
            , ::seq(hana::true_c, hana::true_c)
            , ::seq(hana::true_c, hana::false_c)
            , ::seq(hana::false_c, hana::true_c)
            , ::seq(hana::false_c, hana::false_c)
        );
        hana::test::TestSearchable<::Seq>{bools, hana::make_tuple(hana::true_c, hana::false_c)};
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Functor, Applicative, Monad
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_MONAD
    hana::test::TestFunctor<::Seq>{eqs, eq_keys};
    hana::test::TestApplicative<::Seq>{eqs};
    hana::test::TestMonad<::Seq>{eqs, nested_eqs};
#endif

    //////////////////////////////////////////////////////////////////////////
    // MonadPlus
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_MONAD_PLUS
    hana::test::TestMonadPlus<::Seq>{eqs, predicates, eq_keys};
#endif

    //////////////////////////////////////////////////////////////////////////
    // Sequence
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_SEQUENCE
    hana::test::TestSequence<::Seq>{};
#endif
}
