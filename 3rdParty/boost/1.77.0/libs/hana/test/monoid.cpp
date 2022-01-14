// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/monoid.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/zero.hpp>

#include <laws/monoid.hpp>
namespace hana = boost::hana;


int main() {
    hana::test::TestMonoid<int>{hana::make_tuple(0,1,2,3,4,5)};
    hana::test::TestMonoid<unsigned int>{hana::make_tuple(0u,1u,2u,3u,4u,5u)};
    hana::test::TestMonoid<long>{hana::make_tuple(0l,1l,2l,3l,4l,5l)};
    hana::test::TestMonoid<unsigned long>{hana::make_tuple(0ul,1ul,2ul,3ul,4ul,5ul)};

    // zero
    static_assert(hana::zero<int>() == 0, "");

    // plus
    static_assert(hana::plus(6, 4) == 6 + 4, "");
}
