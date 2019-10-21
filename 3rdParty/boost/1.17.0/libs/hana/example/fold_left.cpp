// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
#include <string>
namespace hana = boost::hana;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

int main() {
    auto f = [=](std::string s, auto element) {
        return "f(" + s + ", " + to_string(element) + ")";
    };

    // with an initial state
    BOOST_HANA_RUNTIME_CHECK(
        hana::fold_left(hana::make_tuple(2, '3', 4, 5.0), "1", f)
            ==
        "f(f(f(f(1, 2), 3), 4), 5)"
    );

    // without initial state
    BOOST_HANA_RUNTIME_CHECK(
        hana::fold_left(hana::make_tuple("1", 2, '3', 4, 5.0), f)
            ==
        "f(f(f(f(1, 2), 3), 4), 5)"
    );
}
