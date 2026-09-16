////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/Match/PatternTypes.h"
#include "Aql/types.h"

#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class Ast;
struct AstNode;
class ExecutionNode;
class ExecutionPlan;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

class FilterBuilder;

using VariableSubstitution = std::unordered_map<VariableId, Variable const*>;

/// @brief Resolves MATCH collections and builds EnumerateCollection access.
/// Uses a single shared enumeration preamble for vertex and edge scans
class CollectionAccessBuilder {
 public:
  CollectionAccessBuilder(ExecutionPlan& plan, Ast* ast,
                          FilterBuilder& filters);

  [[nodiscard]] static std::string requireCollectionName(DataSource const& ds);

  AstNode* buildEdgeCollectionList(NormalizedEdge const& edge);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createCollectionAccess(NormalizedVertex const& vertex,
                         Variable const* fullDocumentVariable,
                         VariableSubstitution const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createPatternEdgeEnumerateAccess(NormalizedEdge const& edge,
                                   Variable const* outputVariable,
                                   VariableSubstitution const& subst);

 private:
  /// @brief Shared logic for enumerating collections and applying
  /// property/WHERE filters for both vertex and edge scans
  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  enumerateCollection(DataSource const& dataSource,
                      Variable const* outputVariable,
                      std::vector<PropertyConstraint> const& properties,
                      std::optional<ExpressionRef> const& filter,
                      VariableSubstitution const& subst);

  ExecutionPlan& _plan;
  Ast* _ast;
  FilterBuilder& _filters;
};

}  // namespace arangodb::aql::match
