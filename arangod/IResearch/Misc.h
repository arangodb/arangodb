////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_IRESEARCH__IRESEARCH_MISC_H
#define ARANGOD_IRESEARCH__IRESEARCH_MISC_H 1

#include <type_traits>

namespace arangodb {
namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
/// @returns true if values from the specified range [Min;Max] are adjacent,
///          false otherwise
////////////////////////////////////////////////////////////////////////////////
template <typename T>
struct adjacencyChecker {
  typedef typename std::enable_if<std::is_enum<T>::value, T>::type type_t;

  template <type_t Max>
  static constexpr bool checkAdjacency() noexcept {
    return true;
  }

  template <type_t Max, type_t Min, type_t... Types>
  static constexpr bool checkAdjacency() noexcept {
    typedef typename std::underlying_type<type_t>::type underlying_t;

    return (Max > Min) && (1 == (underlying_t(Max) - underlying_t(Min))) &&
           checkAdjacency<Min, Types...>();
  }
};  // adjacencyCheker

}  // namespace iresearch
}  // namespace arangodb

#endif  // ARANGOD_IRESEARCH__IRESEARCH_MISC_H
