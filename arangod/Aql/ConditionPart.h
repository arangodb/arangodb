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

#include "Aql/AstNode.h"
#include "Aql/types.h"
#include "Basics/AttributeNameParser.h"

#include <string>
#include <vector>

namespace arangodb::aql {

struct Variable;

/// @brief side on which an attribute occurs in a condition
enum AttributeSideType { ATTRIBUTE_LEFT, ATTRIBUTE_RIGHT };

/// @brief result of comparing two AND-joined condition parts
enum ConditionPartCompareResult {
  IMPOSSIBLE = 0,
  SELF_CONTAINED_IN_OTHER = 1,
  OTHER_CONTAINED_IN_SELF = 2,
  DISJOINT = 3,
  CONVERT_EQUAL = 4,
  UNKNOWN = 5
};
using CompareResult = ConditionPartCompareResult;

/// @brief subsumption lookup tables.
/// Indexed as [cmp(x,y)+1][otherOp][selfOp], where otherOp/selfOp come from
/// ConditionPart::whichCompareOperation(). The rule block documenting the
/// entries lives next to the definitions in ConditionPart.cpp.
extern CompareResult const ResultsTable[3][7][7];
extern CompareResult const ResultsTableMultiValued[3][7][7];

/// @brief clears the attribute access data
void clearAttributeAccess(
    std::pair<Variable const*, std::vector<basics::AttributeName>>& parts);

struct ConditionPart {
  ConditionPart() = delete;

  ConditionPart(Variable const*, std::string const&, AstNode const*,
                AttributeSideType, void*);

  ConditionPart(Variable const*, std::vector<basics::AttributeName> const&,
                AstNode const*, AttributeSideType, void*);

  ~ConditionPart();

  int whichCompareOperation() const noexcept;

  /// @brief returns the lower bound
  AstNode const* lowerBound() const;

  /// @brief returns if the lower bound is inclusive
  bool isLowerInclusive() const noexcept;

  /// @brief returns the upper bound
  AstNode const* upperBound() const;

  /// @brief returns if the upper bound is inclusive
  bool isUpperInclusive() const noexcept;

  /// @brief true if the condition is completely covered by the other condition
  bool isCoveredBy(ConditionPart const& other, bool isReversed) const;

  Variable const* variable;
  std::string attributeName;
  AstNodeType operatorType;
  bool isExpanded;
  AstNode const* operatorNode;
  AstNode const* valueNode;
  void* data;
};

}  // namespace arangodb::aql
