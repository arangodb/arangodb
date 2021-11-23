// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/type.hpp>

#include <support/constexpr_move_only.hpp>
#include <support/tracked_move_only.hpp>

#include <utility>
namespace hana = boost::hana;


constexpr bool in_constexpr_context() {
    auto t0 = hana::make_set(ConstexprMoveOnly<2>{}, ConstexprMoveOnly<3>{});
    auto t_implicit = std::move(t0);
    auto t_explicit(std::move(t_implicit));

    (void)t_implicit;
    (void)t_explicit;
    return true;
}

static_assert(in_constexpr_context(), "");


int main() {
    {
        auto t0 = hana::make_set();
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_explicit;
        (void)t_implicit;
    }
    {
        auto t0 = hana::make_set(TrackedMoveOnly<1>{});
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_set(TrackedMoveOnly<1>{}, TrackedMoveOnly<2>{});
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
    {
        auto t0 = hana::make_set(TrackedMoveOnly<1>{}, TrackedMoveOnly<2>{}, TrackedMoveOnly<3>{});
        auto t_implicit = std::move(t0);
        auto t_explicit(std::move(t_implicit));

        (void)t_implicit;
        (void)t_explicit;
    }
}
