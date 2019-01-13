// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/euclidean_ring.hpp>
#include <laws/group.hpp>
#include <laws/monoid.hpp>
#include <laws/ring.hpp>
namespace hana = boost::hana;


int main() {
    constexpr auto ints = hana::make_tuple(
        hana::int_c<-10>,
        hana::int_c<-2>,
        hana::int_c<0>,
        hana::int_c<1>,
        hana::int_c<3>,
        hana::int_c<4>
    );

    hana::test::TestMonoid<hana::integral_constant_tag<int>>{ints};
    hana::test::TestGroup<hana::integral_constant_tag<int>>{ints};
    hana::test::TestRing<hana::integral_constant_tag<int>>{ints};
    hana::test::TestEuclideanRing<hana::integral_constant_tag<int>>{ints};
}
