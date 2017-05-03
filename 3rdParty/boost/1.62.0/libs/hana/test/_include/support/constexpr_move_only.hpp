// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#ifndef TEST_SUPPORT_CONSTEXPR_MOVE_ONLY_HPP
#define TEST_SUPPORT_CONSTEXPR_MOVE_ONLY_HPP

#include <boost/hana/bool.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/type.hpp>

#include <utility>


// A move-only type that's also a literal type. It is also Comparable and
// Hashable so it can be used in associative containers.
template <int i>
struct ConstexprMoveOnly {
    constexpr ConstexprMoveOnly() { }
    constexpr ConstexprMoveOnly(ConstexprMoveOnly const&) = delete;
    constexpr ConstexprMoveOnly& operator=(ConstexprMoveOnly const&) = delete;
    constexpr ConstexprMoveOnly(ConstexprMoveOnly&&) { }
};

template <int i, int j>
constexpr auto operator==(ConstexprMoveOnly<i> const&, ConstexprMoveOnly<j> const&)
{ return boost::hana::bool_c<i == j>; }

template <int i, int j>
constexpr auto operator!=(ConstexprMoveOnly<i> const&, ConstexprMoveOnly<j> const&)
{ return boost::hana::bool_c<i != j>; }

namespace boost { namespace hana {
    template <int i>
    struct hash_impl<ConstexprMoveOnly<i>> {
        static constexpr auto apply(ConstexprMoveOnly<i> const&)
        { return hana::type_c<ConstexprMoveOnly<i>>; };
    };
}}

#endif // !TEST_SUPPORT_CONSTEXPR_MOVE_ONLY_HPP
