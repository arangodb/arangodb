// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/tuple.hpp>

#include <utility>
namespace hana = boost::hana;


// Make sure that the tuple(Yn&&...) is not preferred over the
// tuple(tuple<Yn...> const&) and the tuple(tuple<Yn...>&&)
// constructors when copy-constructing a tuple with a single
// element that can be constructed from tuple<Yn...> const& and
// tuple<Yn...>&&, respectively.

struct Trap1 {
    Trap1() = default;
    Trap1(Trap1 const&) = default;
    Trap1(Trap1&) = default;
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
    Trap2(Trap2&) = default;
    Trap2(Trap2&&) = default;

    template <typename X>
    Trap2(X) { // not by reference
        static_assert(sizeof(X) && false,
        "this constructor must not be instantiated");
    }
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
}
