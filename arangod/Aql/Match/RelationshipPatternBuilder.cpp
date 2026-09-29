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

#include "Aql/Match/RelationshipPatternBuilder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Match/CollectionAccessBuilder.h"
#include "Aql/Match/FilterBuilder.h"
#include "Aql/Match/PathConstruction.h"
#include "Aql/Match/PatternBuildState.h"
#include "Aql/Match/ProjectionBuilder.h"
#include "Aql/Match/VertexPatternBuilder.h"
#include "Aql/QueryContext.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Basics/debugging.h"
#include "Graph/TraverserOptions.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace arangodb::aql::match {
namespace {

void applyPathRange(PathRange const& range,
                    traverser::TraverserOptions& options) {
  if (range.isDefaultFixedOne()) {
    options.minDepth = 1;
    options.maxDepth = 1;
    return;
  }

  options.minDepth = range.minDepth();
  if (range.hasMaxDepth()) {
    options.maxDepth = range.maxDepth();
  } else {
    // kUnboundedMin: no finite upper bound in the normalized representation.
    options.maxDepth = std::numeric_limits<uint64_t>::max();
  }
}

}  // namespace

RelationshipPatternBuilder::RelationshipPatternBuilder(
    ExecutionPlan& plan, Ast* ast, FilterBuilder& filters,
    CollectionAccessBuilder& collections, ProjectionBuilder& projections,
    PathConstruction& paths, VertexPatternBuilder& vertices)
    : _plan(plan),
      _ast(ast),
      _filters(filters),
      _collections(collections),
      _projections(projections),
      _paths(paths),
      _vertices(vertices) {}

void RelationshipPatternBuilder::emitSegment(NormalizedSegment const& segment,
                                             PatternBuildState& state) {
  ADB_PROD_ASSERT(state.prevVar != nullptr);
  auto const& edge = segment.edge;
  auto const& target = segment.target;

  if (edge.range.isDefaultFixedOne()) {
    emitFixedLength(edge, target, state);
  } else {
    emitVariableLength(edge, target, state);
  }
}

void RelationshipPatternBuilder::emitFixedLength(NormalizedEdge const& edge,
                                                 PatternElement const& target,
                                                 PatternBuildState& state) {
  if (edge.collections.size() > 1) {
    emitFixedLengthMultiCollection(edge, target, state);
  } else {
    emitFixedLengthSingleCollection(edge, target, state);
  }
}

void RelationshipPatternBuilder::emitFixedLengthMultiCollection(
    NormalizedEdge const& edge, PatternElement const& target,
    PatternBuildState& state) {
  // Multi-collection one-hop: same projection temp/subst pattern as the
  // single-collection join path. Substitutions must be registered before
  // later elements rewrite aliases that may reference these variables.
  auto& subst = state.variableScope.map();
  auto edgeBinding =
      bindProjectedVariable(_ast, edge.variable, edge.projection, subst);

  auto const targetOutputs = _vertices.prepareTargetOutputs(target, state);

  auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
      state.prevVar, edge, target, edgeBinding.fullDocument,
      targetOutputs.documentOutputVariable, subst);

  firstNode->addDependency(state.previous);
  state.previous = state.en = lastNode;

  // Filters must see the full edge document (pre-projection).
  if (!edge.properties.empty() || edge.filter.has_value()) {
    auto [propCalc, propFilter] = _filters.createPropertiesFilter(
        edgeBinding.fullDocument, edge.properties, edge.filter, subst);
    propCalc->addDependency(state.previous);
    state.previous = state.en = propFilter;
  }

  maybeQueueEdgeProjection(_projections, state.projections, edgeBinding, subst);
  if (targetOutputs.binding.hasProjection()) {
    ADB_PROD_ASSERT(rightVertexVar == targetOutputs.binding.fullDocument);
    _vertices.queueTargetProjection(targetOutputs.binding, state);
  }

  state.prevVar = rightVertexVar;
  _paths.addPathEdge(state.pathEdges, edgeBinding.destination);
  _paths.addPathVertex(state.pathVertices, targetOutputs.destinationVariable);
}

void RelationshipPatternBuilder::emitFixedLengthSingleCollection(
    NormalizedEdge const& edge, PatternElement const& target,
    PatternBuildState& state) {
  ExecutionNode* lastNodeFilter = nullptr;
  Variable const* edgeVar = nullptr;

  auto& subst = state.variableScope.map();
  auto edgeBinding =
      bindProjectedVariable(_ast, edge.variable, edge.projection, subst);

  std::tie(state.en, lastNodeFilter, edgeVar) =
      _collections.createPatternEdgeEnumerateAccess(
          edge, edgeBinding.fullDocument, subst);
  state.en->addDependency(state.previous);
  state.previous = state.en = lastNodeFilter;

  maybeQueueEdgeProjection(_projections, state.projections, edgeBinding, subst);

  Variable const* leftVertexVar = state.prevVar;
  Variable const* rightVertexVar = nullptr;
  Variable const* vertexDestinationVariable = nullptr;

  if (target.kind == PatternElement::Kind::kVariableReference) {
    rightVertexVar = target.variableReference;
    vertexDestinationVariable = rightVertexVar;
  } else {
    ADB_PROD_ASSERT(target.kind == PatternElement::Kind::kVertex);
    ADB_PROD_ASSERT(target.vertex.has_value());

    auto const targetOutputs = _vertices.prepareTargetOutputs(target, state);
    vertexDestinationVariable = targetOutputs.destinationVariable;
    rightVertexVar = _vertices.emitTargetCollectionAccess(
        *target.vertex, targetOutputs.binding, state);
  }

  auto [firstNode, lastNode] = _filters.createVertexEdgeFilter(
      leftVertexVar, edgeVar, rightVertexVar, edge.direction);
  firstNode->addDependency(state.previous);
  state.previous = state.en = lastNode;
  state.prevVar = rightVertexVar;

  _paths.addPathEdge(state.pathEdges, edgeBinding.destination);
  _paths.addPathVertex(state.pathVertices, vertexDestinationVariable);
}

void RelationshipPatternBuilder::emitVariableLength(
    NormalizedEdge const& edge, PatternElement const& target,
    PatternBuildState& state) {
  // Variable-length: edge.variable is a path object, so edge-document
  // RETURN projections do not apply here. Target vertex projections do.
  auto const targetOutputs = _vertices.prepareTargetOutputs(target, state);

  auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
      state.prevVar, edge, target, /*edgeDocumentOutputVariable*/ nullptr,
      targetOutputs.documentOutputVariable, state.variableScope.map());

  firstNode->addDependency(state.previous);
  state.previous = state.en = lastNode;

  if (targetOutputs.binding.hasProjection()) {
    ADB_PROD_ASSERT(rightVertexVar == targetOutputs.binding.fullDocument);
    _vertices.queueTargetProjection(targetOutputs.binding, state);
  }

  state.prevVar = rightVertexVar;

  _paths.appendTraversalPath(state.pathVertices, state.pathEdges,
                             edge.variable);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
RelationshipPatternBuilder::createTraversalForPattern(
    Variable const* startNodeVar, NormalizedEdge const& edge,
    PatternElement const& target, Variable const* edgeDocumentOutputVariable,
    Variable const* vertexDocumentOutputVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  aql::QueryContext& query = _ast->query();
  auto options = std::make_unique<traverser::TraverserOptions>(query);
  applyPathRange(edge.range, *options);

  auto dirNode = _ast->createNodeValueInt(std::invoke(
      [](EdgeDirection d) {
        switch (d) {
          case EdgeDirection::kInbound:
            return 1;
          case EdgeDirection::kOutbound:
            return 2;
          case EdgeDirection::kAny:
            return 0;
        }
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "invalid direction for match expression");
      },
      edge.direction));

  auto* startNode = _ast->createNodeReference(startNodeVar);
  auto* edgeCollectionList = _collections.buildEdgeCollectionList(edge);
  auto* graphNode = _ast->createNodeCollectionList(edgeCollectionList,
                                                   _ast->query().resolver());

  auto* traversal = _plan.createNode<TraversalNode>(
      &_plan, _plan.nextId(), &_ast->query().vocbase(), dirNode, startNode,
      graphNode, nullptr, std::move(options));

  bool const fixedDepth = edge.range.isDefaultFixedOne();
  if (fixedDepth) {
    ADB_PROD_ASSERT(edgeDocumentOutputVariable != nullptr);
    traversal->setEdgeOutput(edgeDocumentOutputVariable);
  } else {
    // Variable length: edge.variable receives the path object; individual
    // edge documents go to an unused temporary.
    traversal->setPathOutput(edge.variable);
    auto traversalEdgeOutputVar = _ast->variables()->createTemporaryVariable();
    traversal->setEdgeOutput(traversalEdgeOutputVar);
  }

  switch (target.kind) {
    case PatternElement::Kind::kVariableReference: {
      auto const* traversalVertexOutputVar =
          _ast->variables()->createTemporaryVariable();
      traversal->setVertexOutput(traversalVertexOutputVar);
      auto rightVertexVar = target.variableReference;

      auto traversalOutputVertexId =
          _filters.createPropertyAccess(traversalVertexOutputVar, "_id");
      auto rightVertexId = _filters.createPropertyAccess(rightVertexVar, "_id");
      auto condition = _ast->createNodeBinaryOperator(
          NODE_TYPE_OPERATOR_BINARY_EQ, rightVertexId, traversalOutputVertexId);
      auto const* filterVar = _ast->variables()->createTemporaryVariable();
      CalculationNode* calc = _plan.createNode<CalculationNode>(
          &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, condition),
          filterVar);
      calc->addDependency(traversal);
      FilterNode* filter =
          _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
      filter->addDependency(calc);

      return std::make_tuple(traversal, filter, rightVertexVar);
    }
    case PatternElement::Kind::kVertex: {
      ADB_PROD_ASSERT(target.vertex.has_value());
      ADB_PROD_ASSERT(vertexDocumentOutputVariable != nullptr);
      auto const& vertex = *target.vertex;

      auto traversalVertexOutputVar = vertexDocumentOutputVariable;
      traversal->setVertexOutput(traversalVertexOutputVar);

      auto traversalVertexOutputId =
          _filters.createPropertyAccess(traversalVertexOutputVar, "_id");
      auto vertexCollectionName =
          CollectionAccessBuilder::requireCollectionName(vertex.collection);
      char const* registeredCollectionName =
          _ast->resources().registerString(vertexCollectionName);

      auto args = _ast->createNodeArray();
      args->addMember(traversalVertexOutputId);
      args->addMember(_ast->createNodeValueString(
          registeredCollectionName, vertexCollectionName.length()));

      auto root =
          _ast->createNodeFunctionCall("IS_SAME_COLLECTION", args, true);
      auto const* filterVar = _ast->variables()->createTemporaryVariable();
      CalculationNode* calc = _plan.createNode<CalculationNode>(
          &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
          filterVar);
      calc->addDependency(traversal);
      FilterNode* filter =
          _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
      filter->addDependency(calc);

      // COR-959: apply target vertex {props}/WHERE on the full document
      // (pre-projection) while still inside the traversal fragment.
      ExecutionNode* lastNode = filter;
      if (!vertex.properties.empty() || vertex.filter.has_value()) {
        auto [propCalc, propFilter] = _filters.createPropertiesFilter(
            traversalVertexOutputVar, vertex.properties, vertex.filter, subst);
        propCalc->addDependency(lastNode);
        lastNode = propFilter;
      }

      return std::make_tuple(traversal, lastNode, traversalVertexOutputVar);
    }
  }

  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "unexpected match expression member");
  return std::make_tuple(nullptr, nullptr, nullptr);
}

}  // namespace arangodb::aql::match
