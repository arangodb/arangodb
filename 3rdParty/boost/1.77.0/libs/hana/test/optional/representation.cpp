// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/optional.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct Foo { };

int main() {
    auto just = hana::just(Foo{});
    static_assert(std::is_same<decltype(just), hana::optional<Foo>>{}, "");

    auto nothing = hana::nothing;
    static_assert(std::is_same<decltype(nothing), hana::optional<>>{}, "");
}
