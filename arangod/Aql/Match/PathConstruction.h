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

/// @brief Assembles MATCH path AST fragments via COR-891 path accumulator
/// helpers and materializes the final path CalculationNode when requested.
class PathConstruction {
 public:
  PathConstruction(ExecutionPlan& plan, Ast* ast);

  void addPathVertex(std::vector<AstNode const*>& pathVertices,
                     Variable const* variable);

  void addPathEdge(std::vector<AstNode const*>& pathEdges,
                   Variable const* variable);

  void appendTraversalPath(std::vector<AstNode const*>& pathVertices,
                           std::vector<AstNode const*>& pathEdges,
                           Variable const* traversalPathVariable);

  CalculationNode* constructPathObject(
      Variable const* outVariable, std::vector<AstNode const*> const& vertices,
      std::vector<AstNode const*> const& edges);

  /// @brief Attach deferred projections, then optionally the path object.
  ExecutionNode* finalizePattern(
      ExecutionNode* previous, std::vector<ExecutionNode*> const& projections,
      Variable const* pathVariable,
      std::vector<AstNode const*> const& pathVertices,
      std::vector<AstNode const*> const& pathEdges);

 private:
  AstNode* constructArray(std::vector<AstNode const*> const& vars);

  ExecutionPlan& _plan;
  Ast* _ast;
};

}  // namespace arangodb::aql::match
