// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/bool.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>

#include <support/constexpr_move_only.hpp>
#include <support/tracked_move_only.hpp>

#include <string>
#include <utility>
namespace hana = boost::hana;


constexpr bool in_constexpr_context() {
    auto t0 = hana::make_map(
        hana::make_pair(ConstexprMoveOnly<2>{}, ConstexprMoveOnly<20>{}),
        hana::make_pair(ConstexprMoveOnly<3>{}, ConstexprMoveOnly<30>{}));
    auto t_implicit = std::move(t0);
    auto t_explicit(std::move(t_implicit));

    (void)t_implicit;
    (void)t_explicit;
    return true;
}

static_assert(in_constexpr_context(), "");


int main() {
    {
        auto t0 = hana::make_map();
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_explicit;
        (void)t_implicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(TrackedMoveOnly<1>{}, TrackedMoveOnly<10>{}));
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(TrackedMoveOnly<1>{}, TrackedMoveOnly<10>{}),
                                 hana::make_pair(TrackedMoveOnly<2>{}, TrackedMoveOnly<20>{}));
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(TrackedMoveOnly<1>{}, TrackedMoveOnly<10>{}),
                                 hana::make_pair(TrackedMoveOnly<2>{}, TrackedMoveOnly<20>{}),
                                 hana::make_pair(TrackedMoveOnly<3>{}, TrackedMoveOnly<30>{}));
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_map(hana::make_pair(hana::int_c<2>, std::string{"abcdef"}));
        auto moved = std::move(t0);
        BOOST_HANA_RUNTIME_CHECK(
            moved == hana::make_map(hana::make_pair(hana::int_c<2>, std::string{"abcdef"}))
        );
    }
}
