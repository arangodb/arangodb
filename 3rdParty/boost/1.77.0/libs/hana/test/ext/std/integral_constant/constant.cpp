// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/ext/std/integral_constant.hpp>

#include <boost/hana/tuple.hpp>
#include <boost/hana/value.hpp>

#include <laws/constant.hpp>

#include <type_traits>
namespace hana = boost::hana;


int main() {
    // value
    static_assert(hana::value(std::integral_constant<int, 0>{}) == 0, "");
    static_assert(hana::value(std::integral_constant<int, 1>{}) == 1, "");
    static_assert(hana::value(std::integral_constant<int, 3>{}) == 3, "");

    // laws
    hana::test::TestConstant<hana::ext::std::integral_constant_tag<int>>{
        hana::make_tuple(
            std::integral_constant<int, -10>{},
            std::integral_constant<int, -2>{},
            std::integral_constant<int, 0>{},
            std::integral_constant<int, 1>{},
            std::integral_constant<int, 3>{}
        ),
        hana::tuple_t<int, long, long long>
    };
}
