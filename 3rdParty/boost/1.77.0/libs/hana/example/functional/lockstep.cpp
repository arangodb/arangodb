// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/lockstep.hpp>
#include <boost/hana/plus.hpp>
namespace hana = boost::hana;


constexpr int to_int(char c) {
    return static_cast<int>(c) - 48;
}

constexpr int increment(int i) {
    return i + 1;
}

static_assert(hana::lockstep(hana::plus)(to_int, increment)('3', 4) == 3 + 5, "");

int main() { }
