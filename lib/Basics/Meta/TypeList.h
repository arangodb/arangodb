////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <type_traits>
#include <variant>

namespace arangodb::meta {

template<typename... Ts>
struct TypeList;

namespace detail {
template<typename A, typename B>
struct Union;

template<typename... As, typename... Bs>
struct Union<TypeList<As...>, TypeList<Bs...>> {
  using type = TypeList<As..., Bs...>;
};

template<typename T>
struct TypeListFromVariant;

template<typename... Ts>
struct TypeListFromVariant<std::variant<Ts...>> {
  using type = TypeList<Ts...>;
};
}  // namespace detail

template<typename... Ts>
struct TypeList {
  template<typename T>
  static bool consteval contains() {
    return (std::is_same_v<T, Ts> || ...);
  }

  template<typename T>
  static std::size_t consteval index() {
    static_assert(contains<T>(), "Type not found in the list");
    size_t i = 0;
    bool found = false;
    auto match = [&]() {
      found = true;
      return i;
    };
    auto nomatch = [&]() { return found ? i : ++i; };
    ((std::is_same_v<T, Ts> ? match() : nomatch()), ...);
    return i;
  }

  static constexpr std::size_t size() noexcept { return sizeof...(Ts); }

  template<typename Func>
  static void foreach (Func&& func) {
    (func.template operator()<Ts>(), ...);
  }

  template<typename... TTs>
  using Append = TypeList<Ts..., TTs...>;

  template<typename List>
  using Union = detail::Union<TypeList, List>::type;

  using asVariant = std::variant<Ts...>;
};

template<typename T>
using TypeListFromVariant = detail::TypeListFromVariant<T>::type;

}  // namespace arangodb::meta
