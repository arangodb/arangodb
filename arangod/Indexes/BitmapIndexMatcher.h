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

#include "Containers/HashSet.h"
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

/// Boolean expression over the elements of the indexed array attribute.
/// Every expression only matches documents whose attribute is an array that
/// contains at least one searched element, so it can be answered from the
/// bitmaps alone; negations only appear as exclusions of an And.
struct TermExpr {
  enum class Type { Term, And, Or };

  Type type = Type::Term;
  /// Term: node evaluating to the searched scalar
  aql::AstNode const* value = nullptr;
  /// And: all must match; Or: one must match
  std::vector<TermExpr> children;
  /// And only: documents matching any of these are removed
  std::vector<TermExpr> excluded;

  /// number of searched elements, for cost estimation
  std::size_t numTerms() const;
};

/// Parts of an AND condition a bitmap index can answer. The last index field
/// is the array attribute, all others are equality prefixes.
struct ConditionParts {
  /// per prefix field, the node holding the compared value
  std::vector<aql::AstNode const*> prefixValues;
  /// conjunction of all supported conditions on the array attribute
  TermExpr expr{.type = TermExpr::Type::And};
  /// positions of the used members in the AND node, ascending
  std::vector<std::size_t> members;
};

/// Match an n-ary AND node. Every prefix field needs an equality and at
/// least one member must require some searched element to be present.
/// Searched elements must be constant scalars; prefix values may come from
/// outside variables.
///
/// Supported conditions on the array attribute, also nested with AND, OR and
/// NOT (an OR may not mix required and excluded elements):
///   x IN doc.attr, x NOT IN doc.attr,
///   [..] ALL IN / ANY IN / NONE IN doc.attr,
///   [..] ALL NOT IN / ANY NOT IN / NONE NOT IN doc.attr
std::optional<ConditionParts> matchCondition(Index const& index,
                                             aql::AstNode const* andNode,
                                             aql::Variable const* reference);

Index::FilterCosts supportsFilterCondition(Index const& index,
                                           aql::AstNode const* node,
                                           aql::Variable const* reference,
                                           std::size_t itemsInIndex);

aql::AstNode* specializeCondition(Index const& index, aql::AstNode* node,
                                  aql::Variable const* reference);

/// Positions of the members of `andNode` that `indexCondition` (the
/// specialized condition the index is used with) answers exactly, so they
/// need not be evaluated again on the returned documents.
containers::HashSet<std::size_t> coveredMembers(
    Index const& index, aql::AstNode const* andNode,
    aql::AstNode const* indexCondition, aql::Variable const* reference);

}  // namespace bitmap_index
}  // namespace arangodb
