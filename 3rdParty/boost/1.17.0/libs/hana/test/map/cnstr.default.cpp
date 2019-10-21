// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/fwd/hash.hpp>
#include <boost/hana/map.hpp>
#include <boost/hana/pair.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


struct NoDefault {
    NoDefault() = delete;
    NoDefault(NoDefault const&) = default;
    constexpr explicit NoDefault(int) { }
};

auto operator==(NoDefault const&, NoDefault const&) { return hana::true_c; }
auto operator!=(NoDefault const&, NoDefault const&) { return hana::false_c; }

// Note: It is also useful to check with a non-empty class, because that
//       triggers different instantiations due to EBO.
struct NoDefault_nonempty {
    NoDefault_nonempty() = delete;
    NoDefault_nonempty(NoDefault_nonempty const&) = default;
    constexpr explicit NoDefault_nonempty(int k) : i(k) { }
    int i;
};

auto operator==(NoDefault_nonempty const&, NoDefault_nonempty const&) { return hana::true_c; }
auto operator!=(NoDefault_nonempty const&, NoDefault_nonempty const&) { return hana::false_c; }

struct Default {
    Default() = default;
    Default(Default const&) = default;
    constexpr explicit Default(int) { }
};

auto operator==(Default const&, Default const&) { return hana::true_c; }
auto operator!=(Default const&, Default const&) { return hana::false_c; }

namespace boost { namespace hana {
    template <>
    struct hash_impl<NoDefault> {
        static constexpr auto apply(NoDefault const&)
        { return hana::type_c<NoDefault>; };
    };

    template <>
    struct hash_impl<NoDefault_nonempty> {
        static constexpr auto apply(NoDefault_nonempty const&)
        { return hana::type_c<NoDefault_nonempty>; };
    };

    template <>
    struct hash_impl<Default> {
        static constexpr auto apply(Default const&)
        { return hana::type_c<Default>; };
    };
}}

int main() {
    auto map1 = hana::make_map(hana::make_pair(Default(1), Default(1)));
    static_assert(std::is_default_constructible<decltype(map1)>::value, "");

    auto map2 = hana::make_map(hana::make_pair(NoDefault(1), NoDefault(1)));
    static_assert(!std::is_default_constructible<decltype(map2)>::value, "");

    auto map3 = hana::make_map(hana::make_pair(NoDefault_nonempty(1), NoDefault_nonempty(1)));
    static_assert(!std::is_default_constructible<decltype(map3)>::value, "");
}
