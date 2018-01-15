// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/array.hpp>

#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/searchable.hpp>

#include <array>
namespace hana = boost::hana;


template <int ...i>
constexpr auto array() { return std::array<int, sizeof...(i)>{{i...}}; }

int main() {
    auto eq_arrays = hana::make_tuple(
          std::array<hana::test::ct_eq<0>, 0>{}
        , std::array<hana::test::ct_eq<0>, 1>{}
        , std::array<hana::test::ct_eq<0>, 2>{}
        , std::array<hana::test::ct_eq<0>, 3>{}
        , std::array<hana::test::ct_eq<0>, 4>{}
    );

    auto eq_keys = hana::make_tuple(hana::test::ct_eq<0>{});

    hana::test::TestSearchable<hana::ext::std::array_tag>{eq_arrays, eq_keys};
}
