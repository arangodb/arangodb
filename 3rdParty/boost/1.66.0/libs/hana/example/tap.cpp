// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/chain.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/tap.hpp>
#include <boost/hana/tuple.hpp>

#include <set>
namespace hana = boost::hana;


int main() {
    // We use a sorted container because the order in which the functions
    // are called is unspecified.
    std::set<int> before, after;

    auto xs = hana::make_tuple(1, 2, 3)
        | hana::tap<hana::tuple_tag>([&](int x) { before.insert(x); })
        | [](auto x) { return hana::make_tuple(x, -x); }
        | hana::tap<hana::tuple_tag>([&](int x) { after.insert(x); });

    BOOST_HANA_RUNTIME_CHECK(before == std::set<int>{1, 2, 3});
    BOOST_HANA_RUNTIME_CHECK(after == std::set<int>{1, -1, 2, -2, 3, -3});
    BOOST_HANA_RUNTIME_CHECK(xs == hana::make_tuple(1, -1, 2, -2, 3, -3));
}
