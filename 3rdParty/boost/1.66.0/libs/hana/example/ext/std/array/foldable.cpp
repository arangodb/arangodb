// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/unpack.hpp>

#include <array>
namespace hana = boost::hana;


int main() {
    std::array<int, 5> a = {{0, 1, 2, 3, 4}};

    auto b = hana::unpack(a, [](auto ...i) {
        return std::array<int, sizeof...(i)>{{(i + 10)...}};
    });

    BOOST_HANA_RUNTIME_CHECK(b == std::array<int, 5>{{10, 11, 12, 13, 14}});
}
