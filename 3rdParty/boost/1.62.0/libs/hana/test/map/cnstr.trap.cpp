// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/detail/wrong.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>

#include <utility>
namespace hana = boost::hana;


// This test makes sure that we do not instantiate rogue constructors when
// doing copies and moves

template <int i>
struct Trap {
    Trap() = default;
    Trap(Trap const&) = default;
    Trap(Trap&) = default;
    Trap(Trap&&) = default;

    template <typename X>
    Trap(X&&) {
        static_assert(hana::detail::wrong<X>{},
        "this constructor must not be instantiated");
    }
};

template <int i, int j>
constexpr auto operator==(Trap<i> const&, Trap<j> const&)
{ return hana::bool_c<i == j>; }

template <int i, int j>
constexpr auto operator!=(Trap<i> const&, Trap<j> const&)
{ return hana::bool_c<i != j>; }

namespace boost { namespace hana {
    template <int i>
    struct hash_impl<Trap<i>> {
        static constexpr auto apply(Trap<i> const&)
        { return hana::type_c<Trap<i>>; };
    };
}}

int main() {
    {
        auto expr = hana::make_map(
            hana::make_pair(Trap<0>{}, Trap<0>{})
        );
        auto implicit_copy = expr;
        decltype(expr) explicit_copy(expr);

        (void)implicit_copy;
        (void)explicit_copy;
    }
    {
        auto expr = hana::make_map(
            hana::make_pair(Trap<0>{}, Trap<0>{})
        );
        auto implicit_move = std::move(expr);
        decltype(expr) explicit_move(std::move(implicit_move));

        (void)implicit_move;
        (void)explicit_move;
    }
}
