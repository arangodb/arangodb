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

#include "Aql/Match/VertexPatternBuilder.h"

#include "Aql/Ast.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/Match/CollectionAccessBuilder.h"
#include "Aql/Match/PathConstruction.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/Variable.h"
#include "Basics/debugging.h"

namespace arangodb::aql::match {

VertexPatternBuilder::VertexPatternBuilder(ExecutionPlan& /*plan*/, Ast* ast,
                                           CollectionAccessBuilder& collections,
                                           ProjectionBuilder& projections,
                                           PathConstruction& paths)
    : _ast(ast),
      _collections(collections),
      _projections(projections),
      _paths(paths) {}

void VertexPatternBuilder::emitStart(PatternElement const& start,
                                     PatternBuildState& state) {
  if (start.kind == PatternElement::Kind::kVertex) {
    ADB_PROD_ASSERT(start.vertex.has_value());
    emitStartVertex(*start.vertex, state);
    return;
  }

  ADB_PROD_ASSERT(start.kind == PatternElement::Kind::kVariableReference);
  state.prevVar = state.variableScope.resolve(start.variableReference);
  _paths.addPathVertex(state.pathVertices, state.prevVar);
}

void VertexPatternBuilder::emitStartVertex(NormalizedVertex const& vertex,
                                           PatternBuildState& state) {
  auto& subst = state.variableScope.map();
  auto binding =
      bindProjectedVariable(_ast, vertex.variable, vertex.projection, subst);

  ExecutionNode* lastNode = nullptr;
  std::tie(state.en, lastNode, state.prevVar) =
      _collections.createCollectionAccess(vertex, binding.fullDocument, subst);
  state.en->addDependency(state.previous);
  state.previous = state.en = lastNode;

  _paths.addPathVertex(state.pathVertices, binding.destination);
  maybeQueueDocumentProjection(_projections, state.projections, binding, subst);
}

TargetVertexOutputs VertexPatternBuilder::prepareTargetOutputs(
    PatternElement const& target, PatternBuildState& state) {
  TargetVertexOutputs outputs;
  if (target.kind == PatternElement::Kind::kVariableReference) {
    outputs.destinationVariable = target.variableReference;
    outputs.documentOutputVariable = nullptr;
    return outputs;
  }

  ADB_PROD_ASSERT(target.kind == PatternElement::Kind::kVertex);
  ADB_PROD_ASSERT(target.vertex.has_value());
  outputs.binding = bindProjectedVariable(_ast, target.vertex->variable,
                                          target.vertex->projection,
                                          state.variableScope.map());
  outputs.destinationVariable = outputs.binding.destination;
  outputs.documentOutputVariable = outputs.binding.fullDocument;
  return outputs;
}

Variable const* VertexPatternBuilder::emitTargetCollectionAccess(
    NormalizedVertex const& vertex, ProjectionBinding const& binding,
    PatternBuildState& state) {
  ExecutionNode* lastNodeFilter = nullptr;
  Variable const* rightVertexVar = nullptr;
  std::tie(state.en, lastNodeFilter, rightVertexVar) =
      _collections.createCollectionAccess(vertex, binding.fullDocument,
                                          state.variableScope.map());
  state.en->addDependency(state.previous);

  maybeQueueDocumentProjection(_projections, state.projections, binding,
                               state.variableScope.map());

  state.previous = state.en = lastNodeFilter;
  return rightVertexVar;
}

void VertexPatternBuilder::queueTargetProjection(
    ProjectionBinding const& binding, PatternBuildState& state) {
  maybeQueueDocumentProjection(_projections, state.projections, binding,
                               state.variableScope.map());
}

}  // namespace arangodb::aql::match
