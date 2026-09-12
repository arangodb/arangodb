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

#include "Aql/MatchCollectionAccessBuilder.h"
#include "Aql/MatchFilterBuilder.h"
#include "Aql/MatchPatternTypes.h"
#include "Aql/MatchProjectionBuilder.h"
#include "Aql/types.h"

#include <cstddef>
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

/// @brief Orchestrates lowering of normalized MATCH patterns into
/// ExecutionPlan fragments. Collection access, filtering, projection, and
/// variable substitution are delegated to focused helpers; this class
/// coordinates pattern construction and plan wiring.
class MatchBuilder {
 public:
  MatchBuilder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, AstNode const* matchNode);

 private:
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
      MatchPatternElement const& target,
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
  MatchFilterBuilder _filters;
  MatchCollectionAccessBuilder _collections;
  MatchProjectionBuilder _projections;
};

}  // namespace arangodb::aql
