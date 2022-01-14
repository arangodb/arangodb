// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/config.hpp>
#include <boost/hana/first.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/second.hpp>
namespace hana = boost::hana;


constexpr auto in_constexpr_context(int a, short b) {
    hana::pair<int, short> p1(a, b);
    hana::pair<int, short> p2;
    hana::pair<double, long> p3;
    p2 = p1;
    p3 = p2;
    return p3;
}

int main() {
    {
        hana::pair<int, short> p1(3, 4);
        hana::pair<double, long> p2;
        p2 = p1;
        BOOST_HANA_RUNTIME_CHECK(hana::first(p2) == 3);
        BOOST_HANA_RUNTIME_CHECK(hana::second(p2) == 4);
    }

    // make sure that also works in a constexpr context
    // (test fails under GCC <= 6 due to buggy constexpr)
#if BOOST_HANA_CONFIG_GCC >= BOOST_HANA_CONFIG_VERSION(7, 0, 0)
    {
        constexpr auto p = in_constexpr_context(3, 4);
        static_assert(hana::first(p) == 3, "");
        static_assert(hana::second(p) == 4, "");
    }
#endif
}
