// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/equal.hpp>
#include <boost/hana/monadic_fold_left.hpp>
#include <boost/hana/optional.hpp>
#include <boost/hana/traits.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <type_traits>
namespace hana = boost::hana;


auto builtin_common_t = hana::sfinae([](auto&& t, auto&& u) -> decltype(hana::type_c<
    std::decay_t<decltype(true ? hana::traits::declval(t) : hana::traits::declval(u))>
>) { return {}; });

template <typename ...T>
struct common_type { };

template <typename T, typename U>
struct common_type<T, U>
    : std::conditional_t<std::is_same<std::decay_t<T>, T>{} &&
                         std::is_same<std::decay_t<U>, U>{},
        decltype(builtin_common_t(hana::type_c<T>, hana::type_c<U>)),
        common_type<std::decay_t<T>, std::decay_t<U>>
    >
{ };

template <typename T1, typename ...Tn>
struct common_type<T1, Tn...>
    : decltype(hana::monadic_fold_left<hana::optional_tag>(
        hana::tuple_t<Tn...>,
        hana::type_c<std::decay_t<T1>>,
        hana::sfinae(hana::metafunction<common_type>)
    ))
{ };

template <typename ...Ts>
using common_type_t = typename common_type<Ts...>::type;

static_assert(std::is_same<
    common_type_t<char, short, char, short>,
    int
>{}, "");

static_assert(std::is_same<
    common_type_t<char, double, short, char, short, double>,
    double
>{}, "");

static_assert(std::is_same<
    common_type_t<char, short, float, short>,
    float
>{}, "");

static_assert(
    hana::sfinae(hana::metafunction<common_type>)(
        hana::type_c<int>, hana::type_c<int>, hana::type_c<int*>
    ) == hana::nothing
, "");

int main() { }
