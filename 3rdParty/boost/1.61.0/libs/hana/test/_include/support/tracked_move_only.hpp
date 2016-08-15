// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_TRACKED_MOVE_ONLY_HPP
#define TEST_SUPPORT_TRACKED_MOVE_ONLY_HPP

#include <boost/hana/bool.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/type.hpp>

#include <support/tracked.hpp>

#include <utility>


// A move-only type that's Tracked. It is also Comparable and Hashable so it
// can be used in associative containers.
template <int i>
struct TrackedMoveOnly : Tracked {
    TrackedMoveOnly() : Tracked(i) { }
    TrackedMoveOnly(TrackedMoveOnly const&) = delete;
    TrackedMoveOnly& operator=(TrackedMoveOnly const&) = delete;
    TrackedMoveOnly(TrackedMoveOnly&& x)
        : Tracked(std::move(x))
    { }
};

template <int i, int j>
constexpr auto operator==(TrackedMoveOnly<i> const&, TrackedMoveOnly<j> const&)
{ return boost::hana::bool_c<i == j>; }

template <int i, int j>
constexpr auto operator!=(TrackedMoveOnly<i> const&, TrackedMoveOnly<j> const&)
{ return boost::hana::bool_c<i != j>; }

namespace boost { namespace hana {
    template <int i>
    struct hash_impl<TrackedMoveOnly<i>> {
        static constexpr auto apply(TrackedMoveOnly<i> const&)
        { return hana::type_c<TrackedMoveOnly<i>>; };
    };
}}

#endif // !TEST_SUPPORT_TRACKED_MOVE_ONLY_HPP
