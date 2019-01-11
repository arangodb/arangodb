// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/ext/std/integer_sequence.hpp>
#include <boost/hana/unpack.hpp>

#include <tuple>
#include <utility>
namespace hana = boost::hana;


auto add = [](auto a, auto b, auto c) { return a + b + c; };

int main() {
    std::tuple<int, long, double> tuple{1, 2l, 3.3};

    auto sum = hana::unpack(std::integer_sequence<int, 0, 1, 2>{}, [&](auto ...i) {
        // the `i`s are `std::integral_constant<int, ...>`s
        return add(std::get<decltype(i)::value>(tuple)...);
    });

    BOOST_HANA_RUNTIME_CHECK(sum == 6.3);
}
