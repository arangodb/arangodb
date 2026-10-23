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
/// Negations only appear as exclusions of an And. Expressions made of Term,
/// And (with at least one required child) and Or only match documents that
/// contain a searched element, so they can be answered from the bitmaps
/// alone.
///
/// The remaining types only appear when matching evaluated runtime values:
/// True and False are constants (e.g. an empty array with ALL IN), Unknown is
/// a searched value that is never indexed (an array or object element).
struct TermExpr {
  enum class Type { Term, And, Or, True, False, Unknown };

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
/// Constant searched elements must be scalars; prefix values may come from
/// outside variables.
///
/// Supported conditions on the array attribute, also nested with AND, OR and
/// NOT (an OR may not mix required and excluded elements):
///   x IN doc.attr, x NOT IN doc.attr,
///   [..] ALL IN / ANY IN / NONE IN doc.attr,
///   [..] ALL NOT IN / ANY NOT IN / NONE NOT IN doc.attr
///
/// At planning time (`runtime == false`) searched values may also be
/// expressions not depending on `reference`; their conditions must still be
/// evaluated on the returned documents. At runtime these have been replaced
/// by their values, which may be arrays of any content.
std::optional<ConditionParts> matchCondition(Index const& index,
                                             aql::AstNode const* andNode,
                                             aql::Variable const* reference,
                                             bool runtime = false);

Index::FilterCosts supportsFilterCondition(Index const& index,
                                           aql::AstNode const* node,
                                           aql::Variable const* reference,
                                           std::size_t itemsInIndex);

aql::AstNode* specializeCondition(Index const& index, aql::AstNode* node,
                                  aql::Variable const* reference);

/// Positions of the members of `andNode` that `indexCondition` (the
/// specialized condition the index is used with) answers exactly, so they
/// need not be evaluated again on the returned documents. Conditions with
/// searched values only known at runtime are never answered exactly.
containers::HashSet<std::size_t> coveredMembers(
    Index const& index, aql::AstNode const* andNode,
    aql::AstNode const* indexCondition, aql::Variable const* reference);

/// Positions of the members of `andNode` on the indexed array attribute
/// whose searched values are only known at runtime. These must stay in the
/// filter even if generic optimizer rules consider them covered.
containers::HashSet<std::size_t> runtimeMembers(
    Index const& index, aql::AstNode const* andNode,
    aql::Variable const* reference);

}  // namespace bitmap_index
}  // namespace arangodb
