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

#include <tuple>
#include <unordered_map>

namespace arangodb::aql {
class Ast;
class ExecutionNode;
class ExecutionPlan;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

class CollectionAccessBuilder;
class FilterBuilder;
struct PatternBuildState;
class PathConstruction;
class ProjectionBuilder;
class VertexPatternBuilder;

/// @brief Relationship / edge MATCH pattern construction (COR-892).
/// Separates fixed-length and variable-length relationship lowering.
class RelationshipPatternBuilder {
 public:
  RelationshipPatternBuilder(ExecutionPlan& plan, Ast* ast,
                             FilterBuilder& filters,
                             CollectionAccessBuilder& collections,
                             ProjectionBuilder& projections,
                             PathConstruction& paths,
                             VertexPatternBuilder& vertices);

  /// @brief Emit one normalized MATCH segment (edge + target).
  void emitSegment(NormalizedSegment const& segment, PatternBuildState& state);

 private:
  void emitFixedLength(NormalizedEdge const& edge, PatternElement const& target,
                       PatternBuildState& state);

  void emitFixedLengthMultiCollection(NormalizedEdge const& edge,
                                      PatternElement const& target,
                                      PatternBuildState& state);

  void emitFixedLengthSingleCollection(NormalizedEdge const& edge,
                                       PatternElement const& target,
                                       PatternBuildState& state);

  void emitVariableLength(NormalizedEdge const& edge,
                          PatternElement const& target,
                          PatternBuildState& state);

  std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
  createTraversalForPattern(
      Variable const* startNodeVar, NormalizedEdge const& edge,
      PatternElement const& target, Variable const* edgeDocumentOutputVariable,
      Variable const* vertexDocumentOutputVariable,
      std::unordered_map<VariableId, Variable const*> const& subst);

  ExecutionPlan& _plan;
  Ast* _ast;
  FilterBuilder& _filters;
  CollectionAccessBuilder& _collections;
  ProjectionBuilder& _projections;
  PathConstruction& _paths;
  VertexPatternBuilder& _vertices;
};

}  // namespace arangodb::aql::match
