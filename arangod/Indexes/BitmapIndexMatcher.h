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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Indexes/Index.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace arangodb {
namespace aql {
struct AstNode;
struct Variable;
}  // namespace aql

namespace bitmap_index {

/// Parts of an AND condition a bitmap index can answer. The last index field
/// is the array attribute, all others are equality prefixes.
struct ConditionParts {
  /// per prefix field, the node holding the compared value
  std::vector<aql::AstNode const*> prefixValues;
  /// nodes holding the searched elements: an array for `x ALL IN doc.attr`,
  /// a scalar for `x IN doc.attr`
  std::vector<aql::AstNode const*> termValues;
  /// positions of the used members in the AND node, ascending
  std::vector<std::size_t> members;
  /// number of searched elements known at planning time
  std::size_t numTerms = 0;
};

/// Match an n-ary AND node. Every prefix field needs an equality and at
/// least one condition on the array attribute must be present. Searched
/// elements must be constant; prefix values may come from outside variables.
std::optional<ConditionParts> matchCondition(Index const& index,
                                             aql::AstNode const* andNode,
                                             aql::Variable const* reference);

Index::FilterCosts supportsFilterCondition(Index const& index,
                                           aql::AstNode const* node,
                                           aql::Variable const* reference,
                                           std::size_t itemsInIndex);

aql::AstNode* specializeCondition(Index const& index, aql::AstNode* node,
                                  aql::Variable const* reference);

}  // namespace bitmap_index
}  // namespace arangodb
