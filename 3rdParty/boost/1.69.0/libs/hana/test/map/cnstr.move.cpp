// Copyright Louis Dionne 2013-2017
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
#include <type_traits>
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


struct NoMove {
    NoMove() = default;
    NoMove(NoMove const&) = delete;
    NoMove(NoMove&&) = delete;
    friend auto operator==(NoMove const&, NoMove const&) { return hana::true_c; }
    friend auto operator!=(NoMove const&, NoMove const&) { return hana::false_c; }
};

// Note: It is also useful to check with a non-empty class, because that
//       triggers different instantiations due to EBO.
struct NoMove_nonempty {
    NoMove_nonempty() = default;
    NoMove_nonempty(NoMove_nonempty const&) = delete;
    NoMove_nonempty(NoMove_nonempty&&) = delete;
    int i;
    friend auto operator==(NoMove_nonempty const&, NoMove_nonempty const&) { return hana::true_c; }
    friend auto operator!=(NoMove_nonempty const&, NoMove_nonempty const&) { return hana::false_c; }
};

namespace boost { namespace hana {
    template <>
    struct hash_impl<NoMove> {
        static constexpr auto apply(NoMove const&)
        { return hana::type_c<NoMove>; };
    };

    template <>
    struct hash_impl<NoMove_nonempty> {
        static constexpr auto apply(NoMove_nonempty const&)
        { return hana::type_c<NoMove_nonempty>; };
    };
}}

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

    {
        using Map1 = hana::map<hana::pair<NoMove, NoMove>>;
        Map1 map1;
        static_assert(!std::is_move_constructible<Map1>::value, "");

        using Map2 = hana::map<hana::pair<NoMove_nonempty, NoMove_nonempty>>;
        Map2 map2;
        static_assert(!std::is_move_constructible<Map2>::value, "");
    }
}
