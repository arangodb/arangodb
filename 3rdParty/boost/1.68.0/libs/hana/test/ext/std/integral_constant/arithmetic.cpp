// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/integral_constant.hpp>

#include <boost/hana/tuple.hpp>

#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/monoid.hpp>
#include <laws/ring.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    auto ints = hana::make_tuple(
        std::integral_constant<int, -10>{},
        std::integral_constant<int, -2>{},
        std::integral_constant<int, 0>{},
        std::integral_constant<int, 1>{},
        std::integral_constant<int, 3>{}
    );

    hana::test::TestMonoid<hana::ext::std::integral_constant_tag<int>>{ints};
    hana::test::TestGroup<hana::ext::std::integral_constant_tag<int>>{ints};
    hana::test::TestRing<hana::ext::std::integral_constant_tag<int>>{ints};
    hana::test::TestEuclideanRing<hana::ext::std::integral_constant_tag<int>>{ints};
}
