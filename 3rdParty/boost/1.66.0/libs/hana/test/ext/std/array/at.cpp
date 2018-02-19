// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/array.hpp>

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>

#include <laws/base.hpp> // for move_only

#include <array>
namespace hana = boost::hana;


int main() {
    // Check non-const lvalue reference
    {
        std::array<int, 2> arr = {{999, 888}};
        int& i = hana::at_c<0>(arr);
        BOOST_HANA_RUNTIME_CHECK(i == 999);
        arr[0] = 333;
        BOOST_HANA_RUNTIME_CHECK(i == 333);
        i = 444;
        BOOST_HANA_RUNTIME_CHECK(arr[0] == 444);
    }

    // Check const lvalue reference
    {
        std::array<int, 2> arr = {{999, 888}};
        int const& i = hana::at_c<0>(static_cast<std::array<int, 2> const&>(arr));
        BOOST_HANA_RUNTIME_CHECK(i == 999);
        arr[0] = 333;
        BOOST_HANA_RUNTIME_CHECK(i == 333);
    }

    // Check move-only types
    {
        std::array<hana::test::move_only, 2> arr{};
        hana::test::move_only m = hana::at_c<0>(std::move(arr));
        (void)m;
    }
}
