// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;

//
// Make sure that we can fold_left and take arguments by reference.
//

int main() {
    // with state
    {
        auto xs = hana::make_tuple(1, 2, 3);
        int state = 99;

        int& three = hana::fold_left(xs, state, [](int&, int& i) -> int& {
            return i;
        });
        BOOST_HANA_RUNTIME_CHECK(three == 3);
        three = 10;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(xs) == 10);
    }

    // without state
    {
        auto xs = hana::make_tuple(1, 2, 3);

        int& three = hana::fold_left(xs, [](int&, int& i) -> int& {
            return i;
        });
        BOOST_HANA_RUNTIME_CHECK(three == 3);
        three = 10;
        BOOST_HANA_RUNTIME_CHECK(hana::at_c<2>(xs) == 10);
    }
}
