// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/group.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/group.hpp>
using namespace boost::hana;


int main() {
    test::TestGroup<int>{make<tuple_tag>(0,1,2,3,4,5)};
    test::TestGroup<long>{make<tuple_tag>(0l,1l,2l,3l,4l,5l)};

    // minus
    static_assert(minus(6, 4) == 6 - 4, "");

    // negate
    static_assert(negate(6) == -6, "");
}
