// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/scan_left.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
namespace hana = boost::hana;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

auto f = [](auto state, auto element) {
    return "f(" + to_string(state) + ", " + to_string(element) + ")";
};

int main() {
    // with initial state
    BOOST_HANA_RUNTIME_CHECK(hana::scan_left(hana::make_tuple(2, "3", '4'), 1, f) == hana::make_tuple(
        1,
        "f(1, 2)",
        "f(f(1, 2), 3)",
        "f(f(f(1, 2), 3), 4)"
    ));

    // without initial state
    BOOST_HANA_RUNTIME_CHECK(hana::scan_left(hana::make_tuple(1, "2", '3'), f) == hana::make_tuple(
        1,
        "f(1, 2)",
        "f(f(1, 2), 3)"
    ));
}
