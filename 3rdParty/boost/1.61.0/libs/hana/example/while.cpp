// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/plus.hpp>
#include <boost/hana/while.hpp>

#include <vector>
namespace hana = boost::hana;
using namespace hana::literals;


int main() {
    // while_ with a Constant condition (loop is unrolled at compile-time)
    {
        std::vector<int> ints;
        auto final_state = hana::while_(hana::less.than(10_c), 0_c, [&](auto i) {
            ints.push_back(i);
            return i + 1_c;
        });

        // The state is known at compile-time
        BOOST_HANA_CONSTANT_CHECK(final_state == 10_c);

        BOOST_HANA_RUNTIME_CHECK(ints == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    }

    // while_ with a constexpr or runtime condition (loop is not unrolled)
    {
        std::vector<int> ints;
        int final_state = hana::while_(hana::less.than(10), 0, [&](int i) {
            ints.push_back(i);
            return i + 1;
        });

        // The state is known only at runtime, or at compile-time if constexpr
        BOOST_HANA_RUNTIME_CHECK(final_state == 10);

        BOOST_HANA_RUNTIME_CHECK(ints == std::vector<int>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9});
    }
}
