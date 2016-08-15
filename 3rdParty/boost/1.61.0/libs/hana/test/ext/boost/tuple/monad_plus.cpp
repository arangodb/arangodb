// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/boost/tuple.hpp>

#include <boost/hana/bool.hpp>
#include <boost/hana/functional/always.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/monad_plus.hpp>

#include <boost/tuple/tuple.hpp>
using namespace boost::hana;


template <int i>
using eq = test::ct_eq<i>;

int main() {
    //////////////////////////////////////////////////////////////////////////
    // Setup for the laws below
    //////////////////////////////////////////////////////////////////////////
    auto eq_tuples = make<tuple_tag>(
          ::boost::make_tuple(eq<0>{})
        , ::boost::make_tuple(eq<0>{}, eq<1>{})
        , ::boost::make_tuple(eq<0>{}, eq<1>{}, eq<2>{})
    );

    auto eq_values = make<tuple_tag>(eq<0>{}, eq<1>{}, eq<2>{});
    auto predicates = make<tuple_tag>(
        equal.to(eq<0>{}), equal.to(eq<1>{}), equal.to(eq<2>{}),
        always(false_c), always(true_c)
    );

    //////////////////////////////////////////////////////////////////////////
    // MonadPlus
    //////////////////////////////////////////////////////////////////////////
    test::TestMonadPlus<ext::boost::tuple_tag>{eq_tuples, predicates, eq_values};
}
