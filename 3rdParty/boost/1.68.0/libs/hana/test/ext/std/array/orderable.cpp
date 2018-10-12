// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/array.hpp>

#include <boost/hana/tuple.hpp>

#include <laws/orderable.hpp>

#include <array>
namespace hana = boost::hana;


template <int ...i>
constexpr auto array() { return std::array<int, sizeof...(i)>{{i...}}; }

int main() {
    auto int_arrays = hana::make_tuple(
          array<>()
        , array<0>()
        , array<0, 1>()
        , array<0, 1, 2>()
        , array<0, 1, 2, 3>()
        , array<0, 1, 2, 3, 4>()
    );

    hana::test::TestOrderable<hana::ext::std::array_tag>{int_arrays};
}
