// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/orderable.hpp>
#include <laws/product.hpp>

#include <utility>
namespace hana = boost::hana;
using hana::test::ct_eq;
using hana::test::ct_ord;


int main() {
    auto eq_elems = hana::make_tuple(ct_eq<3>{}, ct_eq<4>{});

    auto eqs = hana::make_tuple(
          std::make_pair(ct_eq<3>{}, ct_eq<3>{})
        , std::make_pair(ct_eq<3>{}, ct_eq<4>{})
        , std::make_pair(ct_eq<4>{}, ct_eq<3>{})
        , std::make_pair(ct_eq<4>{}, ct_eq<4>{})
    );

    auto ords = hana::make_tuple(
          std::make_pair(ct_ord<3>{}, ct_ord<3>{})
        , std::make_pair(ct_ord<3>{}, ct_ord<4>{})
        , std::make_pair(ct_ord<4>{}, ct_ord<3>{})
        , std::make_pair(ct_ord<4>{}, ct_ord<4>{})
    );

    hana::test::TestComparable<hana::ext::std::pair_tag>{eqs};
    hana::test::TestOrderable<hana::ext::std::pair_tag>{ords};
    hana::test::TestFoldable<hana::ext::std::pair_tag>{eqs};
    hana::test::TestProduct<hana::ext::std::pair_tag>{eq_elems};
}
