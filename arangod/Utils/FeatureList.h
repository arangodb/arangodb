////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>

#include <frozen/string.h>
#include <frozen/unordered_map.h>

namespace arangodb {

template<typename T>
struct TypeTag {
  using type = T;

  size_t index;
};

template<typename... T>
class TypeList {
 public:
  constexpr static auto Size = sizeof...(T);

  static constexpr auto toArray() {
    return toArrayImpl(std::make_integer_sequence<size_t, Size>());
  }

  template<typename Visitor, size_t... Idx>
  static constexpr void visit(Visitor&& visitor) {
    visitImpl(std::forward<Visitor>(visitor),
              std::make_integer_sequence<size_t, Size>());
  }

 private:
  template<size_t... Idx>
  static constexpr std::array<std::pair<frozen::string, size_t>, Size>
  toArrayImpl(std::integer_sequence<size_t, Idx...>) {
    return {std::pair<frozen::string, size_t>{T::name(), Idx}...};
  }

  template<typename Visitor, size_t... Idx>
  static constexpr void visitImpl(Visitor&& visitor,
                                  std::integer_sequence<size_t, Idx...>) {
    (std::forward<Visitor>(visitor)(TypeTag<T>{Idx}), ...);
  }
};

template<typename... T>
class FeatureList {
 public:
  template<typename Visitor>
  static constexpr void visit(Visitor&& visitor) {
    Types::visit(std::forward<Visitor>(visitor));
  }

  template<typename U>
  static consteval size_t id() {
    static_assert(kFeatures.find(U::name()) != kFeatures.end(),
                  "Feature not found");
    return kFeatures[U::name()];
  }

 private:
  using Types = TypeList<T...>;

  static constexpr auto kFeatures =
      frozen::make_unordered_map(Types::toArray());
};

}
