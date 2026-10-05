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
#include "Aql/Match/PathConstruction.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/TypedAstNodes.h"
#include "Aql/Match/RelationshipPatternBuilder.h"
#include "Aql/Match/VertexPatternBuilder.h"

namespace arangodb::aql {
class Ast;
struct AstNode;
class ExecutionNode;
class ExecutionPlan;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

/// @brief Orchestrates lowering of normalized MATCH patterns into
/// ExecutionPlan fragments. Vertex, relationship (fixed/variable), and path
/// construction are delegated to focused helpers (COR-892).
class Builder {
 public:
  Builder(ExecutionPlan& plan, Ast* ast);

  /// @brief Lower a NODE_TYPE_MATCH AST node, chaining onto @p previous.
  ExecutionNode* build(ExecutionNode* previous, ast::MatchNode matchNode);

 private:
  Ast* _ast;
  FilterBuilder _filters;
  CollectionAccessBuilder _collections;
  ProjectionBuilder _projections;
  PathConstruction _paths;
  VertexPatternBuilder _vertices;
  RelationshipPatternBuilder _relationships;
};

}  // namespace arangodb::aql::match
