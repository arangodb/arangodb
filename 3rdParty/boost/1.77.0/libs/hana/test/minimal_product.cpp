// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/second.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/orderable.hpp>
#include <laws/product.hpp>
#include <support/minimal_product.hpp>
#include <support/tracked.hpp>

#include <utility>
namespace hana = boost::hana;
using hana::test::ct_eq;
using hana::test::ct_ord;


int main() {
    // Make sure `first` and `second` are "accessors". If they are not,
    // `Tracked` should report a double-move.
    {
        auto prod = ::minimal_product(::Tracked{1}, ::Tracked{2});
        auto fst = hana::first(std::move(prod));
        auto snd = hana::second(std::move(prod));
    }

    //////////////////////////////////////////////////////////////////////////
    // Comparable, Orderable, Foldable, Product
    //////////////////////////////////////////////////////////////////////////
    auto eq_elems = hana::make_tuple(ct_eq<3>{}, ct_eq<4>{});

    auto eqs = hana::make_tuple(
          ::minimal_product(ct_eq<3>{}, ct_eq<3>{})
        , ::minimal_product(ct_eq<3>{}, ct_eq<4>{})
        , ::minimal_product(ct_eq<4>{}, ct_eq<3>{})
        , ::minimal_product(ct_eq<4>{}, ct_eq<4>{})
    );

    auto ords = hana::make_tuple(
          ::minimal_product(ct_ord<3>{}, ct_ord<3>{})
        , ::minimal_product(ct_ord<3>{}, ct_ord<4>{})
        , ::minimal_product(ct_ord<4>{}, ct_ord<3>{})
        , ::minimal_product(ct_ord<4>{}, ct_ord<4>{})
    );

    hana::test::TestComparable<::MinimalProduct>{eqs};
    hana::test::TestOrderable<::MinimalProduct>{ords};
    hana::test::TestFoldable<::MinimalProduct>{eqs};
    hana::test::TestProduct<::MinimalProduct>{eq_elems};
}
