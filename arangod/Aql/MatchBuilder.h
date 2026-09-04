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

#include "Aql/MatchPatternTypes.h"
#include "Aql/types.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {

class Ast;
struct AstNode;
class CalculationNode;
class ExecutionNode;
class ExecutionPlan;
class FilterNode;
struct Variable;

/// @brief Lowers normalized MATCH patterns into ExecutionPlan fragments.
/// Parser AST semantics are normalized by MatchPatternNormalizer before
/// planning; this class owns only execution-plan construction.
class MatchBuilder {
 public:
  MatchBuilder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, AstNode const* matchNode);

 private:
  AstNode* createPropertyAccess(Variable const* variable,
                                std::string_view property);

  AstNode* buildEdgeCollectionList(NormalizedEdge const& edge);

  std::tuple<CalculationNode*, FilterNode*> createPropertiesFilter(
      Variable const* variable,
      std::vector<MatchPropertyConstraint> const& properties,
      std::optional<MatchExpressionRef> const& additionalFilter,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createCollectionAccess(
      NormalizedVertex const& vertex, Variable const* fullDocumentVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionNode* createPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<MatchProjection> const& projection, bool isEdge,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createPatternEdgeEnumerateAccess(
      NormalizedEdge const& edge, Variable const* outputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<CalculationNode*, FilterNode*> createVertexEdgeFilter(
      Variable const* leftVertex, Variable const* edge,
      Variable const* rightVertex, MatchEdgeDirection direction);

  /// @param edgeDocumentOutputVariable Edge document output for fixed-depth
  /// traversals. Ignored when the edge variable receives a path object.
  /// @param vertexDocumentOutputVariable Vertex output when @p target is a
  /// vertex pattern. Ignored for variable reference targets. Callers that
  /// apply MATCH projections must pass temporaries and register substitutions
  /// before later alias rewrites (same ordering as the join lowering path).
  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createTraversalForPattern(Variable const* startNodeVar,
                            NormalizedEdge const& edge,
                            MatchPatternElement const& target,
                            Variable const* edgeDocumentOutputVariable,
                            Variable const* vertexDocumentOutputVariable);

  AstNode* constructArray(std::vector<AstNode const*> const& vars);

  CalculationNode* constructPathObject(
      Variable const* outVariable, std::vector<AstNode const*> const& vertices,
      std::vector<AstNode const*> const& edges);

  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql
