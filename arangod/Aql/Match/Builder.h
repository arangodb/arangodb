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

#include "Aql/Match/CollectionAccessBuilder.h"
#include "Aql/Match/FilterBuilder.h"
#include "Aql/Match/PatternTypes.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/types.h"

#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace arangodb::aql {
class Ast;
struct AstNode;
class CalculationNode;
class ExecutionNode;
class ExecutionPlan;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Orchestrates lowering of normalized MATCH patterns into
/// ExecutionPlan fragments. Collection access, filtering, and projection are
/// delegated to focused helpers; this class coordinates pattern construction
/// and plan wiring (including COR-888 projection bindings).
class Builder {
 public:
  Builder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, ast::MatchNode matchNode);

 private:
  /// @brief User-facing pattern variable plus the variable that holds the full
  /// document during enumeration/traversal (a temporary when projecting).
  /// @p projection points into the NormalizedStatement owned for the
  /// duration of build(); null when not projecting.
  struct ProjectionBinding {
    Variable const* destination{nullptr};
    Variable const* fullDocument{nullptr};
    Projection const* projection{nullptr};

    [[nodiscard]] bool hasProjection() const noexcept {
      return projection != nullptr;
    }
  };

  /// @brief When @p projection is set, create a temporary full-document
  /// variable and register destination→temp in @p subst; otherwise enumerate
  /// directly into @p destination. Stores a pointer to @p projection's value
  /// on the binding (must outlive the binding; true for normalize→build).
  ProjectionBinding bindProjectedVariable(
      Variable const* destination, std::optional<Projection> const& projection,
      std::unordered_map<VariableId, Variable const*>& subst);

  /// @brief Queue a delayed document projection CalculationNode when @p binding
  /// has a projection. Preserves existing ordering (after segment lowering).
  void maybeQueueDocumentProjection(
      std::vector<ExecutionNode*>& projections,
      ProjectionBinding const& binding,
      std::unordered_map<VariableId, Variable const*> const& subst);

  /// @brief Queue a delayed edge-document projection CalculationNode when
  /// @p binding has a projection. Preserves existing ordering (after segment
  /// lowering).
  void maybeQueueEdgeProjection(
      std::vector<ExecutionNode*>& projections,
      ProjectionBinding const& binding,
      std::unordered_map<VariableId, Variable const*> const& subst);

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
      PatternElement const& target, Variable const* edgeDocumentOutputVariable,
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
  FilterBuilder _filters;
  CollectionAccessBuilder _collections;
  ProjectionBuilder _projections;
};

}  // namespace arangodb::aql::match
