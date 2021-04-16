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
#ifndef VELOCYPACK_UTILITIES_H
#define VELOCYPACK_UTILITIES_H
#include <memory>
#include "gadgets.h"

namespace arangodb {
namespace velocypack {
namespace deserializer::utilities {

template <typename T>
struct identity_factory {
  using constructed_type = T;
  T operator()(T t) const { return t; }
};

template <typename P>
struct make_unique_factory {
  using constructed_type = std::unique_ptr<P>;

  template <typename... S>
  auto operator()(S&&... s) -> constructed_type {
    return std::make_unique<P>(std::forward<S>(s)...);
  }
};

template <typename T>
struct constructor_factory {
  using constructed_type = T;

  template <typename... S>
  T operator()(S&&... s) const {
    static_assert(detail::gadgets::is_braces_constructible_v<T, S...>,
                  "the type is not constructable with the given types");
    return T{std::forward<S>(s)...};
  }
};

template<typename F>
struct visiting_factory {
  using constructed_type = typename F::constructed_type;

  template <typename... S>
  auto operator()(std::variant<S...>&& s) const {
    return std::visit([](auto&& p) {
      F f;
      return f(std::forward<decltype(p)>(p));
    }, std::move(s));
  }
};

template <typename T, typename P>
struct constructing_deserializer {
  using constructed_type = T;
  using plan = P;
  using factory = constructor_factory<T>;
};

template <auto value>
struct member_extractor;

template <typename A, typename B, A B::*ptr>
struct member_extractor<ptr> {
  static A& exec(B& b) { return b.*ptr; }

  static A const& exec(B const& b) { return b.*ptr; }
};

struct not_empty_validator {
  template <typename C>
  auto operator()(C&& c) -> std::optional<deserialize_error> {
    if (c.empty()) {
      return deserialize_error{"must not be empty"};
    }
    return {};
  }
};

template <typename>
using always_false = std::false_type;
template <typename T>
constexpr bool always_false_v = always_false<T>::value;

}  // namespace deserializer::utilities
}  // namespace velocypack
}  // namespace arangodb
#endif  // VELOCYPACK_UTILITIES_H
