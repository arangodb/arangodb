// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/set.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


template <int i>
struct NoDefault {
    NoDefault() = delete;
    NoDefault(NoDefault const&) = default;
    constexpr explicit NoDefault(int) { }
};

template <int i, int j>
auto operator==(NoDefault<i> const&, NoDefault<j> const&) { return hana::bool_<i == j>{}; }
template <int i, int j>
auto operator!=(NoDefault<i> const&, NoDefault<j> const&) { return hana::bool_<i != j>{}; }

template <int i>
struct Default {
    Default() = default;
    Default(Default const&) = default;
    constexpr explicit Default(int) { }
};

template <int i, int j>
auto operator==(Default<i> const&, Default<j> const&) { return hana::bool_<i == j>{}; }
template <int i, int j>
auto operator!=(Default<i> const&, Default<j> const&) { return hana::bool_<i != j>{}; }

namespace boost { namespace hana {
    template <int i>
    struct hash_impl<NoDefault<i>> {
        static constexpr auto apply(NoDefault<i> const&)
        { return hana::type_c<NoDefault<i>>; };
    };

    template <int i>
    struct hash_impl<Default<i>> {
        static constexpr auto apply(Default<i> const&)
        { return hana::type_c<Default<i>>; };
    };
}}

int main() {
    {
        auto set0 = hana::make_set();
        static_assert(std::is_default_constructible<decltype(set0)>{}, "");

        auto set1 = hana::make_set(Default<0>(int{}));
        static_assert(std::is_default_constructible<decltype(set1)>{}, "");

        auto set2 = hana::make_set(Default<0>(int{}), Default<1>(int{}));
        static_assert(std::is_default_constructible<decltype(set2)>{}, "");

        auto set3 = hana::make_set(Default<0>(int{}), NoDefault<1>(int{}), Default<2>(int{}));
        static_assert(!std::is_default_constructible<decltype(set3)>{}, "");
    }

    {
        auto set1 = hana::make_set(NoDefault<0>(int{}));
        static_assert(!std::is_default_constructible<decltype(set1)>{}, "");

        auto set2 = hana::make_set(NoDefault<0>(int{}), NoDefault<1>(int{}));
        static_assert(!std::is_default_constructible<decltype(set2)>{}, "");

        auto set3 = hana::make_set(NoDefault<0>(int{}), NoDefault<1>(int{}), NoDefault<2>(int{}));
        static_assert(!std::is_default_constructible<decltype(set3)>{}, "");
    }
}