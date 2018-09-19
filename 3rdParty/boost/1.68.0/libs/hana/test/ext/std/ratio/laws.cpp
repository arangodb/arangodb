// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/ratio.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/comparable.hpp>
#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/monoid.hpp>
#include <laws/orderable.hpp>
#include <laws/ring.hpp>

#include <ratio>
namespace hana = boost::hana;


int main() {
    auto ratios = hana::make_tuple(
          std::ratio<0>{}
        , std::ratio<1, 3>{}
        , std::ratio<1, 2>{}
        , std::ratio<2, 6>{}
        , std::ratio<3, 1>{}
        , std::ratio<7, 8>{}
        , std::ratio<3, 5>{}
        , std::ratio<2, 1>{}
    );

    hana::test::TestComparable<hana::ext::std::ratio_tag>{ratios};
    hana::test::TestOrderable<hana::ext::std::ratio_tag>{ratios};
    hana::test::TestMonoid<hana::ext::std::ratio_tag>{ratios};
    hana::test::TestGroup<hana::ext::std::ratio_tag>{ratios};
    hana::test::TestRing<hana::ext::std::ratio_tag>{ratios};
    hana::test::TestEuclideanRing<hana::ext::std::ratio_tag>{ratios};
}
