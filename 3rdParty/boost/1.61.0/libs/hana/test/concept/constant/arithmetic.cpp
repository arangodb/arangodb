// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include "minimal.hpp"

#include <boost/hana/tuple.hpp>

#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/monoid.hpp>
#include <laws/ring.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto ints = hana::make_tuple(
        minimal_constant<int, -3>{},
        minimal_constant<int, 0>{},
        minimal_constant<int, 1>{},
        minimal_constant<int, 2>{},
        minimal_constant<int, 3>{}
    );

    hana::test::TestMonoid<minimal_constant_tag<int>>{ints};
    hana::test::TestGroup<minimal_constant_tag<int>>{ints};
    hana::test::TestRing<minimal_constant_tag<int>>{ints};
    hana::test::TestEuclideanRing<minimal_constant_tag<int>>{ints};
}
