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
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <type_traits>

namespace arangodb::iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @returns true if values from the specified range [Min;Max] are adjacent,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
template<typename T>
struct adjacencyChecker {
  static_assert(std::is_enum_v<T>);
  using type_t = T;

  template<type_t Max>
  static constexpr bool checkAdjacency() noexcept {
    return true;
  }

  template<type_t Max, type_t Min, type_t... Types>
  static constexpr bool checkAdjacency() noexcept {
    using underlying_t = std::underlying_type_t<type_t>;

    return (Max > Min) && (1 == (underlying_t(Max) - underlying_t(Min))) &&
           checkAdjacency<Min, Types...>();
  }
};

}  // namespace arangodb::iresearch
