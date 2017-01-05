// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <support/seq.hpp>

#include <boost/hana/bool.hpp>
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
using namespace boost::hana;


int main() {
    using boost::hana::size_t;
    using test::ct_eq;
    using test::ct_ord;

    auto eqs = make<tuple_tag>(
          ::seq()
        , ::seq(ct_eq<0>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{})
        , ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{})
    );
    (void)eqs;

    auto nested_eqs = make<tuple_tag>(
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

    auto eq_keys = make<tuple_tag>(ct_eq<0>{}, ct_eq<3>{}, ct_eq<10>{});
    (void)eq_keys;

    auto predicates = make<tuple_tag>(
        equal.to(ct_eq<0>{}), equal.to(ct_eq<3>{}), equal.to(ct_eq<10>{}),
        always(true_c), always(false_c)
    );
    (void)predicates;

    auto ords = make<tuple_tag>(
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
    test::TestComparable<::Seq>{eqs};
    test::TestOrderable<::Seq>{ords};
#endif

#ifdef BOOST_HANA_TEST_ITERABLE
    //////////////////////////////////////////////////////////////////////////
    // Foldable
    //////////////////////////////////////////////////////////////////////////
    test::TestFoldable<::Seq>{eqs};

    //////////////////////////////////////////////////////////////////////////
    // Iterable
    //////////////////////////////////////////////////////////////////////////
    {
        test::TestIterable<::Seq>{eqs};
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Searchable
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_SEARCHABLE
    {
        test::TestSearchable<::Seq>{eqs, eq_keys};

        auto bools = make<tuple_tag>(
              ::seq(true_c)
            , ::seq(false_c)
            , ::seq(true_c, true_c)
            , ::seq(true_c, false_c)
            , ::seq(false_c, true_c)
            , ::seq(false_c, false_c)
        );
        test::TestSearchable<::Seq>{bools, make<tuple_tag>(true_c, false_c)};
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Functor, Applicative, Monad
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_MONAD
    test::TestFunctor<::Seq>{eqs, eq_keys};
    test::TestApplicative<::Seq>{eqs};
    test::TestMonad<::Seq>{eqs, nested_eqs};
#endif

    //////////////////////////////////////////////////////////////////////////
    // MonadPlus
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_MONAD_PLUS
    test::TestMonadPlus<::Seq>{eqs, predicates, eq_keys};
#endif

    //////////////////////////////////////////////////////////////////////////
    // Sequence
    //////////////////////////////////////////////////////////////////////////
#ifdef BOOST_HANA_TEST_SEQUENCE
    test::TestSequence<::Seq>{};
#endif
}
