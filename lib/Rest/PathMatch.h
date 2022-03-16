////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

// TODO Replace the SFINAE stuff with concepts, as soon as our compilers support
//      them.
// TODO Restrict valid suffix types. Probably not before we have reasonable
//      support for constraints & concepts.
// TODO This could be extended further. The first useful thing that comes to
//      mind is matching only a prefix, and get a range with the tail which
//      can be used for further matching.
// TODO Match could get an additional constructor with begin/end arguments.

namespace arangodb::rest {

template<typename T>
constexpr static bool match_constant_v =
    std::is_convertible_v<T, std::string_view>;
template<typename A, typename P = std::remove_reference_t<A>,
         typename T = std::remove_pointer_t<P>>
constexpr static bool match_variable_v =
    std::is_pointer_v<P>&& std::is_assignable_v<T, std::string_view>;
template<typename T>
constexpr static bool valid_match_component_v =
    match_constant_v<T> || match_variable_v<T>;

// TODO Restrict this more sensibly, as soon as we can use concepts.
//      Should be an input range which elements are convertible to string_view,
//      or something like that.
template<typename T>
constexpr static bool valid_match_suffix_v = true;

static_assert(match_variable_v<std::string*>);
static_assert(match_variable_v<std::string_view*>);
static_assert(!match_variable_v<char*>);
static_assert(match_constant_v<std::string>);
static_assert(match_constant_v<std::string_view>);
static_assert(match_constant_v<const char*>);
static_assert(!valid_match_component_v<std::string const*>);

template<typename S, typename... Ts, std::size_t... Idxs>
auto matches_impl(S const& suffixes, std::tuple<Ts...> components,
                  std::index_sequence<Idxs...>) {
  static_assert(valid_match_suffix_v<S>);
  static_assert((valid_match_component_v<Ts> && ...));

  if (suffixes.size() != sizeof...(Ts)) {
    return false;
  }

  auto work = [&]<std::size_t Idx>() -> bool {
    using cur_type =
        typename std::tuple_element<Idx, decltype(components)>::type;
    auto&& component = std::get<Idx>(components);
    auto&& suffix = suffixes[Idx];
    if constexpr (match_constant_v<cur_type>) {
      return std::string_view(suffix) == std::string_view(component);
    } else {
      static_assert(match_variable_v<cur_type>);
      *component = std::string_view(suffix);
      return true;
    }
  };

  return (... && (work.template operator()<Idxs>()));
}

template<typename S, std::enable_if_t<valid_match_suffix_v<S>, bool> = true>
struct Match {
  S const& _suffixes;
  explicit Match(S const& suffixes) : _suffixes(suffixes) {}

  template<typename... Ts>
  auto against(Ts&&... components)
      -> std::enable_if_t<(valid_match_component_v<Ts> && ...), bool> {
    return matches_impl(_suffixes,
                        std::forward_as_tuple(std::forward<Ts>(components)...),
                        std::index_sequence_for<Ts...>());
  }
};

}  // namespace arangodb::rest
