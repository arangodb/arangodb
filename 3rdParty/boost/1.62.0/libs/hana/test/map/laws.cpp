// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/map.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/foldable.hpp>
#include <laws/searchable.hpp>
#include <support/minimal_product.hpp>
namespace hana = boost::hana;


template <int i>
auto key() { return hana::test::ct_eq<i>{}; }

template <int i>
auto val() { return hana::test::ct_eq<-i>{}; }

template <int i, int j>
auto p() { return ::minimal_product(key<i>(), val<j>()); }

int main() {
    auto maps = hana::make_tuple(
        hana::make_map(),
        hana::make_map(p<1, 1>()),
        hana::make_map(p<1, 2>()),
        hana::make_map(p<1, 1>(), p<2, 2>()),
        hana::make_map(p<1, 1>(), p<2, 2>(), p<3, 3>())
    );

    hana::test::TestComparable<hana::map_tag>{maps};
    hana::test::TestSearchable<hana::map_tag>{maps, hana::make_tuple(key<1>(), key<4>())};
    hana::test::TestFoldable<hana::map_tag>{maps};
}
