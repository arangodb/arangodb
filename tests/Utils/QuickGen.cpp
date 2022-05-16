////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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

#include "QuickGen.h"

#include "Random/RandomGenerator.h"

namespace arangodb::tests::quick {

template<>
auto generate<AlphaNumeric>() -> AlphaNumeric {
  constexpr static char alphaNumerics[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "abcdefghijklmnopqrstuvwxyz"
      "0123456789";
  constexpr static auto n = 2 * 26 + 10;
  // remember \0
  static_assert(sizeof(alphaNumerics) == n + 1);
  return {alphaNumerics[RandomGenerator::interval(0, n - 1)]};
}

template<>
auto generate<TRI_col_type_e>() -> TRI_col_type_e {
  return static_cast<TRI_col_type_e>(RandomGenerator::interval(2, 3));
}

}  // namespace arangodb::tests::quick
