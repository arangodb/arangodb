// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/monoid.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/monoid.hpp>
using namespace boost::hana;


int main() {
    test::TestMonoid<int>{make<tuple_tag>(0,1,2,3,4,5)};
    test::TestMonoid<unsigned int>{make<tuple_tag>(0u,1u,2u,3u,4u,5u)};
    test::TestMonoid<long>{make<tuple_tag>(0l,1l,2l,3l,4l,5l)};
    test::TestMonoid<unsigned long>{make<tuple_tag>(0ul,1ul,2ul,3ul,4ul,5ul)};

    // zero
    static_assert(zero<int>() == 0, "");

    // plus
    static_assert(plus(6, 4) == 6 + 4, "");
}
