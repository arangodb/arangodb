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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>

#include "Basics/Common.h"

namespace arangodb {
namespace aql {
struct AstNode;

struct Quantifier {
  static int64_t constexpr NONE = 1;
  static int64_t constexpr ALL = 2;
  static int64_t constexpr ANY = 3;

  /// @brief converts a quantifier string into an int equivalent
  static int64_t fromString(std::string const& value);

  /// @brief converts a quantifier int value into its string equivalent
  static std::string stringify(int64_t value);

  static bool isAllOrNone(AstNode const* quantifier);

  /// @brief determine the min/max number of matches for an array comparison
  static std::pair<size_t, size_t> requiredMatches(size_t inputSize,
                                                   AstNode const* quantifier);
};
}  // namespace aql
}  // namespace arangodb
