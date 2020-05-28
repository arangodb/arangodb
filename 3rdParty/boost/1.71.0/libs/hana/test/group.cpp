// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/group.hpp>
#include <boost/hana/minus.hpp>
#include <boost/hana/negate.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/group.hpp>
namespace hana = boost::hana;


int main() {
    hana::test::TestGroup<int>{hana::make_tuple(0,1,2,3,4,5)};
    hana::test::TestGroup<long>{hana::make_tuple(0l,1l,2l,3l,4l,5l)};

    // minus
    static_assert(hana::minus(6, 4) == 6 - 4, "");

    // negate
    static_assert(hana::negate(6) == -6, "");
}
