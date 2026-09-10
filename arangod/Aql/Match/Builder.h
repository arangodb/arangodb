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

#include <cstddef>
#include <optional>
#include <span>
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
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Lowers normalized MATCH patterns into ExecutionPlan fragments.
/// Parser AST semantics are normalized by PatternNormalizer before
/// planning; this class owns only execution-plan construction.
class Builder {
 public:
  Builder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, AstNode const* matchNode);

 private:
  /// @brief User-facing pattern variable plus the variable that holds the full
  /// document during enumeration/traversal (a temporary when projecting).
  struct ProjectionBinding {
    Variable const* destination{nullptr};
    Variable const* fullDocument{nullptr};
    bool hasProjection{false};
  };

  AstNode* createPropertyAccess(Variable const* variable,
                                std::string_view property);

  AstNode* buildEdgeCollectionList(NormalizedEdge const& edge);

  /// @brief When @p projection is set, create a temporary full-document
  /// variable and register destination→temp in @p subst; otherwise enumerate
  /// directly into @p destination.
  ProjectionBinding bindProjectedVariable(
      Variable const* destination,
      std::optional<Projection> const& projection,
      std::unordered_map<VariableId, Variable const*>& subst);

  /// @brief Queue a delayed projection CalculationNode when @p binding has a
  /// projection. Preserves existing ordering (applied after segment lowering).
  void maybeQueueProjection(
      std::vector<ExecutionNode*>& projections,
      ProjectionBinding const& binding,
      std::optional<Projection> const& projection, bool isEdge,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<CalculationNode*, FilterNode*> createPropertiesFilter(
      Variable const* variable,
      std::vector<PropertyConstraint> const& properties,
      std::optional<ExpressionRef> const& additionalFilter,
      std::unordered_map<VariableId, Variable const*> const& subst);

  /// @brief Shared EnumerateCollection + property/WHERE filter fragment used
  /// by both vertex and edge collection scans.
  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  enumerateCollection(
      DataSource const& dataSource, Variable const* outputVariable,
      std::vector<PropertyConstraint> const& properties,
      std::optional<ExpressionRef> const& filter,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createCollectionAccess(
      NormalizedVertex const& vertex, Variable const* fullDocumentVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionNode* createDocumentPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<Projection> const& projection,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionNode* createEdgeDocumentPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<Projection> const& projection,
      std::unordered_map<VariableId, Variable const*> const& subst);

  /// @brief Shared MATCH projection lowering. @p mandatoryAttributes are
  /// auto-injected and treated as reserved for user projection handling.
  ExecutionNode* createPatternProjection(
      Variable const* destinationVariable, Variable const* fullDocumentVar,
      std::optional<Projection> const& projection,
      std::span<std::string_view const> mandatoryAttributes,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createPatternEdgeEnumerateAccess(
      NormalizedEdge const& edge, Variable const* outputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  std::tuple<CalculationNode*, FilterNode*> createVertexEdgeFilter(
      Variable const* leftVertex, Variable const* edge,
      Variable const* rightVertex, EdgeDirection direction);

  /// @param edgeDocumentOutputVariable Edge document output for fixed-depth
  /// traversals. Ignored when the edge variable receives a path object.
  /// @param vertexDocumentOutputVariable Vertex output when @p target is a
  /// vertex pattern. Ignored for variable reference targets. Callers that
  /// apply MATCH projections must pass temporaries and register substitutions
  /// before later alias rewrites (same ordering as the join lowering path).
  /// @param subst Variable substitutions for target-vertex property/WHERE
  /// filters applied inside the traversal fragment (COR-959).
  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createTraversalForPattern(
      Variable const* startNodeVar, NormalizedEdge const& edge,
      PatternElement const& target,
      Variable const* edgeDocumentOutputVariable,
      Variable const* vertexDocumentOutputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  AstNode* constructArray(std::vector<AstNode const*> const& vars);

  CalculationNode* constructPathObject(
      Variable const* outVariable, std::vector<AstNode const*> const& vertices,
      std::vector<AstNode const*> const& edges);

  void addPathVertex(std::vector<AstNode const*>& pathVertices,
                     Variable const* variable);

  void addPathEdge(std::vector<AstNode const*>& pathEdges,
                   Variable const* variable);

  void appendTraversalPath(std::vector<AstNode const*>& pathVertices,
                           std::vector<AstNode const*>& pathEdges,
                           Variable const* traversalPathVariable);

  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql::match
