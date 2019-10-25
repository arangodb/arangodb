// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/fold_left.hpp>
#include <boost/hana/tuple.hpp>

#include <sstream>
namespace hana = boost::hana;


auto to_string = [](auto x) {
    std::ostringstream ss;
    ss << x;
    return ss.str();
};

auto show = [](auto x, auto y) {
    return "(" + to_string(x) + " + " + to_string(y) + ")";
};

int main() {
    BOOST_HANA_RUNTIME_CHECK(
        hana::fold_left(hana::make_tuple(2, "3", '4'), "1", show) == "(((1 + 2) + 3) + 4)"
    );
}
