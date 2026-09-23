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

#include "Aql/Match/PatternBuildState.h"
#include "Aql/Match/PatternTypes.h"

namespace arangodb::aql {
class Ast;
class ExecutionPlan;
struct Variable;
}  // namespace arangodb::aql

namespace arangodb::aql::match {

class CollectionAccessBuilder;
class PathConstruction;
class ProjectionBuilder;

/// @brief Prepared vertex outputs for a relationship target element.
struct TargetVertexOutputs {
  Variable const* destinationVariable{nullptr};
  /// @brief Full target document variable used for traversal/filtering.
  /// May be temporary when projection is deferred; and is null when the target
  /// is an existing variable reference.
  Variable const* documentOutputVariable{nullptr};
  ProjectionBinding binding{};
};

/// @brief Vertex-specific MATCH pattern construction (COR-892).
class VertexPatternBuilder {
 public:
  VertexPatternBuilder(ExecutionPlan& plan, Ast* ast,
                       CollectionAccessBuilder& collections,
                       ProjectionBuilder& projections, PathConstruction& paths);

  /// @brief Emit the pattern start element (vertex or variable reference).
  void emitStart(PatternElement const& start, PatternBuildState& state);

  /// @brief Register projection temporaries for a relationship target vertex.
  TargetVertexOutputs prepareTargetOutputs(PatternElement const& target,
                                           PatternBuildState& state);

  /// @brief Enumerate a target vertex collection on the fixed-length join path.
  /// Does not overwrite @p state.prevVar (left vertex for the edge filter).
  /// @return Full-document output variable for the target vertex.
  Variable const* emitTargetCollectionAccess(NormalizedVertex const& vertex,
                                             ProjectionBinding const& binding,
                                             PatternBuildState& state);

  void queueTargetProjection(ProjectionBinding const& binding,
                             PatternBuildState& state);

 private:
  void emitStartVertex(NormalizedVertex const& vertex,
                       PatternBuildState& state);

  Ast* _ast;
  CollectionAccessBuilder& _collections;
  ProjectionBuilder& _projections;
  PathConstruction& _paths;
};

}  // namespace arangodb::aql::match
