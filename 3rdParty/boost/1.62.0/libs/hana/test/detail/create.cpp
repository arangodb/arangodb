// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/config.hpp>
#include <boost/hana/detail/create.hpp>

#include <utility>
#include <tuple>
namespace hana = boost::hana;


constexpr hana::detail::create<std::tuple> make_tuple{};
constexpr hana::detail::create<std::pair> make_pair{};

template <typename ...>
struct empty { };

template <typename T>
struct single_holder { T x; };

template <typename T>
struct identity { using type = T; };

template <typename ...T>
using identity_t = typename identity<T...>::type;

int main() {
    static_assert(make_tuple(1, '2', 3.3) == std::make_tuple(1, '2', 3.3), "");
    static_assert(make_pair(1, '2') == std::make_pair(1, '2'), "");

    // should work
    hana::detail::create<empty>{}();
    hana::detail::create<single_holder>{}(1);
    hana::detail::create<single_holder>{}([]{});
    hana::detail::create<identity_t>{}(1);

    // Clang < 3.7.0 fails this test
#if !defined(BOOST_HANA_CONFIG_CLANG) || \
    BOOST_HANA_CONFIG_CLANG >= BOOST_HANA_CONFIG_VERSION(3, 7, 0)
    hana::detail::create<identity_t>{}([]{});
#endif
}
