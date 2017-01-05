// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/functional/curry.hpp>
#include <boost/hana/functional/partial.hpp>
#include <boost/hana/functional/reverse_partial.hpp>
#include <boost/hana/tuple.hpp>

#include <tuple>
#include <type_traits>
namespace hana = boost::hana;


struct DefaultConstructible {
    DefaultConstructible() = default;
};

int main() {
    auto curry_tuple = hana::make_tuple(
        std::make_tuple(hana::curry<2>(DefaultConstructible{})(DefaultConstructible{}))
    );

    auto partial_tuple = hana::make_tuple(
        std::make_tuple(hana::partial(DefaultConstructible{}, DefaultConstructible{}))
    );

    auto reverse_partial_tuple = hana::make_tuple(
        std::make_tuple(hana::reverse_partial(DefaultConstructible{}, DefaultConstructible{}))
    );

    static_assert(std::is_default_constructible<decltype(curry_tuple)>::value, "");
    static_assert(std::is_default_constructible<decltype(partial_tuple)>::value, "");
    static_assert(std::is_default_constructible<decltype(reverse_partial_tuple)>::value, "");
}
