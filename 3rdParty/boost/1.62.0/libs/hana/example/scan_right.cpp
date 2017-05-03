// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/scan_right.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
namespace hana = boost::hana;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

auto f = [](auto element, auto state) {
    return "f(" + to_string(element) + ", " + to_string(state) + ")";
};

int main() {
    // with initial state
    BOOST_HANA_RUNTIME_CHECK(hana::scan_right(hana::make_tuple(1, "2", '3'), 4, f) == hana::make_tuple(
        "f(1, f(2, f(3, 4)))",
        "f(2, f(3, 4))",
        "f(3, 4)",
        4
    ));

    // without initial state
    BOOST_HANA_RUNTIME_CHECK(hana::scan_right(hana::make_tuple(1, "2", '3'), f) == hana::make_tuple(
        "f(1, f(2, 3))",
        "f(2, 3)",
        '3'
    ));
}
