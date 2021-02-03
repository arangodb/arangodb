////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#ifndef VELOCYPACK_GADGETS_H
#define VELOCYPACK_GADGETS_H
#include <cstddef>
#include <optional>
#include <tuple>

namespace arangodb {
namespace velocypack {
namespace deserializer::detail::gadgets {
namespace detail {

template <std::size_t I, typename...>
struct index_of_type;

template <std::size_t I, typename T, typename E, typename... Ts>
struct index_of_type<I, T, E, Ts...> {
  constexpr static auto value = index_of_type<I + 1, T, Ts...>::value;
};

template <std::size_t I, typename T, typename... Ts>
struct index_of_type<I, T, T, Ts...> {
  constexpr static auto value = I;
};

}  // namespace detail

template <typename T, typename... Ts>
using index_of_type = detail::index_of_type<0, T, Ts...>;
template <typename T, typename... Ts>
constexpr const auto index_of_type_v = detail::index_of_type<0, T, Ts...>::value;

static_assert(index_of_type_v<bool, bool> == 0);
static_assert(index_of_type_v<bool, bool, char> == 0);
static_assert(index_of_type_v<char, bool, char> == 1);

template <class... Ts>
struct visitor : Ts... {
  using Ts::operator()...;
};
template <class... Ts>
visitor(Ts...) -> visitor<Ts...>;

template <typename R, typename F, typename T>
struct is_applicable_r;

template <typename R, typename F, typename... Ts>
struct is_applicable_r<R, F, std::tuple<Ts...>> {
  constexpr static bool value = std::is_invocable_r_v<R, F, Ts...>;
};

template <typename T, typename C = void>
class is_complete_type : public std::false_type {};

template <typename T>
class is_complete_type<T, decltype(void(sizeof(T)))> : public std::true_type {};

template <typename T>
constexpr const bool is_complete_type_v = is_complete_type<T>::value;

namespace detail {

template <typename T, typename... Args>
struct constructing_wrapper {};

template <typename T, typename C = void>
struct is_braces_constructible : std::false_type {};

template <typename T, typename... Args>
struct is_braces_constructible<constructing_wrapper<T, Args...>,
                               std::void_t<decltype(T{std::declval<Args>()...})>>
    : std::true_type {};

}  // namespace detail

template <class T, typename... Args>
struct is_braces_constructible
    : detail::is_braces_constructible<detail::constructing_wrapper<T, Args...>> {
};  // check if T{std::declval<Args>()...} is wellformed

template <class T, typename... Args>
constexpr const bool is_braces_constructible_v =
    is_braces_constructible<T, Args...>::value;

namespace detail {
template <typename... Ts>
struct tuple_no_void_impl;

template <typename... Ts, typename E, typename... Es>
struct tuple_no_void_impl<std::tuple<Ts...>, E, Es...> {
  using type = typename tuple_no_void_impl<std::tuple<Ts..., E>, Es...>::type;
};

template <typename... Ts, typename... Es>
struct tuple_no_void_impl<std::tuple<Ts...>, void, Es...> {
  using type = typename tuple_no_void_impl<std::tuple<Ts...>, Es...>::type;
};

template <typename... Ts>
struct tuple_no_void_impl<std::tuple<Ts...>> {
  using type = std::tuple<Ts...>;
};
}  // namespace detail

template <typename... Ts>
struct tuple_no_void {
  using type = typename detail::tuple_no_void_impl<std::tuple<>, Ts...>::type;
};

namespace detail {
template <typename... Ts, typename F, std::size_t... Is>
auto tuple_map_impl(std::tuple<Ts...> t, F&& f, std::index_sequence<Is...>)
    -> std::tuple<std::invoke_result_t<F, Ts>...> {
  using std::get;
  return std::make_tuple(f(std::move(get<Is>(t)))...);
}
}  // namespace detail

template <typename... Ts, typename F>
auto tuple_map(std::tuple<Ts...> t, F&& f)
    -> std::tuple<std::invoke_result_t<F, Ts>...> {
  return detail::tuple_map_impl(std::move(t), std::forward<F>(f),
                                std::index_sequence_for<Ts...>{});
}

template <typename>
struct tuple_to_opts;

template <typename... Ts>
struct tuple_to_opts<std::tuple<Ts...>> {
  using type = std::tuple<std::optional<Ts>...>;
};

template <typename T>
using tuple_to_opts_t = typename tuple_to_opts<T>::type;

template <typename... Ts>
std::tuple<Ts...> unpack_opt_tuple(std::tuple<std::optional<Ts>...> t) {
  return tuple_map(std::move(t),
                   [](auto&& x) { return std::forward<decltype(x)>(x).value(); });
}

}  // namespace deserializer::detail::gadgets
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_GADGETS_H
