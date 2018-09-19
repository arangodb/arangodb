// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/ring.hpp>
#include <boost/hana/mult.hpp>
#include <boost/hana/one.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/ring.hpp>
namespace hana = boost::hana;


int main() {
    hana::test::TestRing<int>{hana::make_tuple(0,1,2,3,4,5)};
    hana::test::TestRing<long>{hana::make_tuple(0l,1l,2l,3l,4l,5l)};

    // one
    static_assert(hana::one<int>() == 1, "");

    // mult
    static_assert(hana::mult(6, 4) == 6 * 4, "");
}
