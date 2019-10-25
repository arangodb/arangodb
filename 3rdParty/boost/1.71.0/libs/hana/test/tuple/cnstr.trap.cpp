// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <utility>
namespace hana = boost::hana;


// Make sure that the tuple(Yn&&...) is not preferred over copy constructors
// in single-element cases and other similar cases.

struct Trap1 {
    Trap1() = default;
    Trap1(Trap1 const&) = default;
#ifndef BOOST_HANA_WORKAROUND_MSVC_MULTIPLECTOR_106654
    Trap1(Trap1&) = default;
#endif
    Trap1(Trap1&&) = default;

    template <typename X>
    Trap1(X&&) {
        static_assert(sizeof(X) && false,
        "this constructor must not be instantiated");
    }
};

struct Trap2 {
    Trap2() = default;
    Trap2(Trap2 const&) = default;
#ifndef BOOST_HANA_WORKAROUND_MSVC_MULTIPLECTOR_106654
    Trap2(Trap2&) = default;
#endif
    Trap2(Trap2&&) = default;

    template <typename X>
    Trap2(X) { // not by reference
        static_assert(sizeof(X) && false,
        "this constructor must not be instantiated");
    }
};

struct Trap3 {
    Trap3() = default;
    Trap3(Trap3 const&) = default;
#ifndef BOOST_HANA_WORKAROUND_MSVC_MULTIPLECTOR_106654
    Trap3(Trap3&) = default;
#endif
    Trap3(Trap3&&) = default;

    template <typename X>
    constexpr explicit Trap3(X&&) { // explicit, and constexpr
        static_assert(sizeof(X) && false,
        "this constructor must not be instantiated");
    }
};

struct Trap4 {
    Trap4() = default;
    template <typename Args>
    constexpr explicit Trap4(Args&&) {
        static_assert(sizeof(Args) && false, "must never be instantiated");
    }

    Trap4(Trap4 const&) = default;
    Trap4(Trap4&&) = default;
};

int main() {
    {
        hana::tuple<Trap1> tuple{};
        hana::tuple<Trap1> implicit_copy = tuple;
        hana::tuple<Trap1> explicit_copy(tuple);
        hana::tuple<Trap1> implicit_move = std::move(tuple);
        hana::tuple<Trap1> explicit_move(std::move(tuple));

        (void)implicit_copy;
        (void)explicit_copy;
        (void)implicit_move;
        (void)explicit_move;
    }

    {
        hana::tuple<Trap2> tuple{};
        hana::tuple<Trap2> implicit_copy = tuple;
        hana::tuple<Trap2> explicit_copy(tuple);
        hana::tuple<Trap2> implicit_move = std::move(tuple);
        hana::tuple<Trap2> explicit_move(std::move(tuple));

        (void)implicit_copy;
        (void)explicit_copy;
        (void)implicit_move;
        (void)explicit_move;
    }

    {
        hana::tuple<Trap3> tuple{};
        hana::tuple<Trap3> implicit_copy = tuple;
        hana::tuple<Trap3> explicit_copy(tuple);
        hana::tuple<Trap3> implicit_move = std::move(tuple);
        hana::tuple<Trap3> explicit_move(std::move(tuple));

        (void)implicit_copy;
        (void)explicit_copy;
        (void)implicit_move;
        (void)explicit_move;
    }

    // Just defining the structure used to cause a failure, because of the
    // explicitly defaulted copy-constructor.
    {
        struct Foo {
            Foo() = default;
            Foo(Foo const&) = default;
            Foo(Foo&&) = default;
            hana::tuple<Trap4> t;
        };
    }
}
