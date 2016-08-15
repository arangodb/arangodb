// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/ring.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/ring.hpp>
using namespace boost::hana;


int main() {
    test::TestRing<int>{make<tuple_tag>(0,1,2,3,4,5)};
    test::TestRing<long>{make<tuple_tag>(0l,1l,2l,3l,4l,5l)};

    // one
    static_assert(one<int>() == 1, "");

    // mult
    static_assert(mult(6, 4) == 6 * 4, "");
}
