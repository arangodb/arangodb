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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace arangodb::aql {
struct AstNode;

struct Quantifier {
  enum class Type {
    kNone = 1,
    kAll = 2,
    kAny = 3,
    kAtLeast = 4,
  };

  /// @brief converts a quantifier string into an int equivalent
  static Type fromString(std::string_view value);

  /// @brief converts a quantifier int value into its string equivalent
  static std::string_view stringify(Type value);

  static bool isAll(AstNode const* quantifier);
  static bool isAny(AstNode const* quantifier);
  static bool isNone(AstNode const* quantifier);
  static bool isAtLeast(AstNode const* quantifier);

  /// @brief determine the min/max number of matches for an array comparison
  static std::pair<size_t, size_t> requiredMatches(size_t inputSize,
                                                   AstNode const* quantifier,
                                                   size_t atLeastValue);

 private:
  static bool isType(AstNode const* quantifier, Type type);
};

}  // namespace arangodb::aql
