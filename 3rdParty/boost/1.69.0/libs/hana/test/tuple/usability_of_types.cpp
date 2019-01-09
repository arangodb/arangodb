// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/at.hpp>
#include <boost/hana/back.hpp>
#include <boost/hana/front.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


// The fact that a reference to a `type<...>` is returned from `front(types)`
// and friends used to break the `decltype(front(types))::type` pattern,
// because we would be trying to fetch `::type` inside a reference. To work
// around this, a unary `operator+` turning a lvalue `type` into a rvalue
// `type` was added.

struct T; struct U; struct V;

int main() {
    auto check = [](auto types) {
        static_assert(std::is_same<
            typename decltype(+hana::front(types))::type, T
        >{}, "");

        static_assert(std::is_same<
            typename decltype(+hana::at(types, hana::size_c<1>))::type, U
        >{}, "");

        static_assert(std::is_same<
            typename decltype(+hana::back(types))::type, V
        >{}, "");
    };

    check(hana::make_tuple(hana::type_c<T>, hana::type_c<U>, hana::type_c<V>));
    check(hana::tuple_t<T, U, V>);
}
