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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Quantifier.h"

#include "Aql/AstNode.h"
#include "Basics/debugging.h"

using namespace arangodb::aql;

/// @brief converts a quantifier string into an int equivalent
int64_t Quantifier::FromString(std::string const& value) {
  if (value == "all") {
    return ALL;
  } else if (value == "any") {
    return ANY;
  } else if (value == "none") {
    return NONE;
  }

  return NONE;
}

/// @brief converts a quantifier int value into its string equivalent
std::string Quantifier::Stringify(int64_t value) {
  if (value == ALL) {
    return "all";
  }
  if (value == ANY) {
    return "any";
  }
  if (value == NONE) {
    return "none";
  }
  return "none";
}

bool Quantifier::IsAllOrNone(AstNode const* quantifier) {
  TRI_ASSERT(quantifier != nullptr);

  if (quantifier->type == NODE_TYPE_QUANTIFIER) {
    auto const value = quantifier->getIntValue(true);
    return (value == Quantifier::ALL || value == Quantifier::NONE);
  }
  return false;
}

/// @brief determine the min/max number of matches for an array comparison
std::pair<size_t, size_t> Quantifier::RequiredMatches(size_t inputSize,
                                                      AstNode const* quantifier) {
  TRI_ASSERT(quantifier != nullptr);

  if (quantifier->type == NODE_TYPE_QUANTIFIER) {
    auto const value = quantifier->getIntValue(true);

    if (value == Quantifier::ALL) {
      return std::make_pair(inputSize, inputSize);
    }
    if (value == Quantifier::ANY) {
      return std::make_pair(inputSize == 0 ? 0 : 1, inputSize);
    }
    if (value == Quantifier::NONE) {
      return std::make_pair(0, 0);
    }
  }

  // won't be reached
  TRI_ASSERT(false);
  return std::make_pair(SIZE_MAX, SIZE_MAX);
}
