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

#include "MatchBuilder.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/MatchPatternNormalizer.h"
#include "Aql/MatchVariableScope.h"
#include "Aql/QueryContext.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Graph/TraverserOptions.h"

#include <functional>
#include <limits>
#include <memory>
#include <utility>

namespace arangodb::aql {
namespace {

void applyPathRange(MatchPathRange const& range,
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

MatchBuilder::MatchBuilder(ExecutionPlan& plan, Ast* ast)
    : _plan(plan),
      _ast(ast),
      _filters(plan, ast),
      _collections(plan, ast, _filters),
      _projections(plan, ast) {}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
MatchBuilder::createTraversalForPattern(
    Variable const* startNodeVar, NormalizedEdge const& edge,
    MatchPatternElement const& target,
    Variable const* edgeDocumentOutputVariable,
    Variable const* vertexDocumentOutputVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  aql::QueryContext& query = _ast->query();
  auto options = std::make_unique<traverser::TraverserOptions>(query);
  applyPathRange(edge.range, *options);

  auto dirNode = _ast->createNodeValueInt(std::invoke(
      [](MatchEdgeDirection d) {
        switch (d) {
          case MatchEdgeDirection::kInbound:
            return 1;
          case MatchEdgeDirection::kOutbound:
            return 2;
          case MatchEdgeDirection::kAny:
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
    case MatchPatternElement::Kind::kVariableReference: {
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
    case MatchPatternElement::Kind::kVertex: {
      ADB_PROD_ASSERT(target.vertex.has_value());
      ADB_PROD_ASSERT(vertexDocumentOutputVariable != nullptr);
      auto const& vertex = *target.vertex;

      auto traversalVertexOutputVar = vertexDocumentOutputVariable;
      traversal->setVertexOutput(traversalVertexOutputVar);

      auto traversalVertexOutputId =
          _filters.createPropertyAccess(traversalVertexOutputVar, "_id");
      auto vertexCollectionName =
          MatchCollectionAccessBuilder::requireCollectionName(
              vertex.collection);
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

AstNode* MatchBuilder::constructArray(std::vector<AstNode const*> const& vars) {
  auto root = _ast->createNodeArray();
  for (auto v : vars) {
    root->addMember(v);
  }
  return root;
}

CalculationNode* MatchBuilder::constructPathObject(
    Variable const* outVariable, std::vector<AstNode const*> const& vertices,
    std::vector<AstNode const*> const& edges) {
  auto root = _ast->createNodeObject();

  root->addMember(
      _ast->createNodeObjectElement("edges", constructArray(edges)));
  root->addMember(
      _ast->createNodeObjectElement("vertices", constructArray(vertices)));

  return _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      outVariable);
}

void MatchBuilder::addPathVertex(std::vector<AstNode const*>& pathVertices,
                                 Variable const* variable) {
  pathVertices.push_back(_ast->createNodeReference(variable));
}

void MatchBuilder::addPathEdge(std::vector<AstNode const*>& pathEdges,
                               Variable const* variable) {
  pathEdges.push_back(_ast->createNodeReference(variable));
}

void MatchBuilder::appendTraversalPath(
    std::vector<AstNode const*>& pathVertices,
    std::vector<AstNode const*>& pathEdges,
    Variable const* traversalPathVariable) {
  pathEdges.push_back(
      _ast->createNodeArraySplice(_ast->createNodeAttributeAccess(
          _ast->createNodeReference(traversalPathVariable), "edges")));

  pathVertices.pop_back();
  pathVertices.push_back(
      _ast->createNodeArraySplice(_ast->createNodeAttributeAccess(
          _ast->createNodeReference(traversalPathVariable), "vertices")));
}

ExecutionNode* MatchBuilder::build(ExecutionNode* previous,
                                   AstNode const* matchNode) {
  MatchPatternNormalizer normalizer(*_ast);
  NormalizedMatchStatement const statement = normalizer.normalize(*matchNode);

  auto en = previous;

  for (auto const& pattern : statement.patterns) {
    Variable const* prevVar = nullptr;
    Variable const* pathVariable = pattern.pathVariable;
    std::vector<AstNode const*> pathVertices;
    std::vector<AstNode const*> pathEdges;
    std::vector<ExecutionNode*> projections;

    MatchVariableScope variableScope;

    auto const handleStartVertex = [&](NormalizedVertex const& vertex) {
      auto destinationVariable = vertex.variable;
      bool const hasProjection = vertex.projection.has_value();
      Variable const* enumOutputVariable =
          hasProjection ? _ast->variables()->createTemporaryVariable()
                        : destinationVariable;
      if (hasProjection) {
        variableScope.registerSubstitution(destinationVariable,
                                           enumOutputVariable);
      }

      ExecutionNode* lastNode;
      std::tie(en, lastNode, prevVar) = _collections.createCollectionAccess(
          vertex, enumOutputVariable, variableScope.map());
      en->addDependency(previous);
      previous = en = lastNode;

      addPathVertex(pathVertices, destinationVariable);

      if (hasProjection) {
        projections.push_back(_projections.createDocumentPatternProjection(
            destinationVariable, prevVar, vertex.projection,
            variableScope.map()));
      }
    };

    if (pattern.start.kind == MatchPatternElement::Kind::kVertex) {
      ADB_PROD_ASSERT(pattern.start.vertex.has_value());
      handleStartVertex(*pattern.start.vertex);
    } else {
      ADB_PROD_ASSERT(pattern.start.kind ==
                      MatchPatternElement::Kind::kVariableReference);
      prevVar = variableScope.resolve(pattern.start.variableReference);
      addPathVertex(pathVertices, prevVar);
    }

    for (auto const& segment : pattern.segments) {
      auto const& edge = segment.edge;
      auto const& target = segment.target;
      ADB_PROD_ASSERT(prevVar != nullptr);

      if (edge.range.isDefaultFixedOne() && edge.collections.size() > 1) {
        // Multi-collection one-hop: same projection temp/subst pattern as the
        // single-collection join path. Substitutions must be registered before
        // later elements rewrite aliases that may reference these variables.
        auto edgeDestinationVariable = edge.variable;
        bool const edgeHasProjection = edge.projection.has_value();
        Variable const* edgeTraversalOutputVariable =
            edgeHasProjection ? _ast->variables()->createTemporaryVariable()
                              : edgeDestinationVariable;
        if (edgeHasProjection) {
          variableScope.registerSubstitution(edgeDestinationVariable,
                                             edgeTraversalOutputVariable);
        }

        Variable const* vertexDestinationVariable = nullptr;
        Variable const* vertexTraversalOutputVariable = nullptr;
        bool vertexHasProjection = false;

        if (target.kind == MatchPatternElement::Kind::kVariableReference) {
          vertexDestinationVariable = target.variableReference;
          vertexTraversalOutputVariable = nullptr;
        } else {
          ADB_PROD_ASSERT(target.kind == MatchPatternElement::Kind::kVertex);
          ADB_PROD_ASSERT(target.vertex.has_value());
          vertexDestinationVariable = target.vertex->variable;
          vertexHasProjection = target.vertex->projection.has_value();
          vertexTraversalOutputVariable =
              vertexHasProjection ? _ast->variables()->createTemporaryVariable()
                                  : vertexDestinationVariable;
          if (vertexHasProjection) {
            variableScope.registerSubstitution(vertexDestinationVariable,
                                               vertexTraversalOutputVariable);
          }
        }

        auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
            prevVar, edge, target, edgeTraversalOutputVariable,
            vertexTraversalOutputVariable, variableScope.map());

        firstNode->addDependency(previous);
        previous = en = lastNode;

        // Filters must see the full edge document (pre-projection).
        if (!edge.properties.empty() || edge.filter.has_value()) {
          auto [propCalc, propFilter] = _filters.createPropertiesFilter(
              edgeTraversalOutputVariable, edge.properties, edge.filter,
              variableScope.map());
          propCalc->addDependency(previous);
          previous = en = propFilter;
        }

        if (edgeHasProjection) {
          projections.push_back(
              _projections.createEdgeDocumentPatternProjection(
                  edgeDestinationVariable, edgeTraversalOutputVariable,
                  edge.projection, variableScope.map()));
        }
        if (vertexHasProjection) {
          projections.push_back(_projections.createDocumentPatternProjection(
              vertexDestinationVariable, rightVertexVar,
              target.vertex->projection, variableScope.map()));
        }

        prevVar = rightVertexVar;
        addPathEdge(pathEdges, edgeDestinationVariable);
        addPathVertex(pathVertices, vertexDestinationVariable);
      } else if (edge.range.isDefaultFixedOne()) {
        ExecutionNode* lastNodeFilter;
        Variable const* edgeVar;

        auto edgeDestinationVariable = edge.variable;
        bool const edgeHasProjection = edge.projection.has_value();
        Variable const* edgeEnumOutputVariable =
            edgeHasProjection ? _ast->variables()->createTemporaryVariable()
                              : edgeDestinationVariable;
        if (edgeHasProjection) {
          variableScope.registerSubstitution(edgeDestinationVariable,
                                             edgeEnumOutputVariable);
        }

        std::tie(en, lastNodeFilter, edgeVar) =
            _collections.createPatternEdgeEnumerateAccess(
                edge, edgeEnumOutputVariable, variableScope.map());
        en->addDependency(previous);
        previous = en = lastNodeFilter;

        if (edgeHasProjection) {
          projections.push_back(
              _projections.createEdgeDocumentPatternProjection(
                  edgeDestinationVariable, edgeEnumOutputVariable,
                  edge.projection, variableScope.map()));
        }

        Variable const* rightVertexVar;
        Variable const* vertexDestinationVariable = nullptr;

        if (target.kind == MatchPatternElement::Kind::kVariableReference) {
          rightVertexVar = target.variableReference;
          vertexDestinationVariable = rightVertexVar;
        } else {
          ADB_PROD_ASSERT(target.kind == MatchPatternElement::Kind::kVertex);
          ADB_PROD_ASSERT(target.vertex.has_value());

          vertexDestinationVariable = target.vertex->variable;
          bool const vertexHasProjection =
              target.vertex->projection.has_value();
          Variable const* vertexEnumOutputVariable =
              vertexHasProjection ? _ast->variables()->createTemporaryVariable()
                                  : vertexDestinationVariable;
          if (vertexHasProjection) {
            variableScope.registerSubstitution(vertexDestinationVariable,
                                               vertexEnumOutputVariable);
          }

          std::tie(en, lastNodeFilter, rightVertexVar) =
              _collections.createCollectionAccess(*target.vertex,
                                                  vertexEnumOutputVariable,
                                                  variableScope.map());
          en->addDependency(previous);

          if (vertexHasProjection) {
            projections.push_back(_projections.createDocumentPatternProjection(
                vertexDestinationVariable, rightVertexVar,
                target.vertex->projection, variableScope.map()));
          }

          previous = en = lastNodeFilter;
        }

        auto [firstNode, lastNode] = _filters.createVertexEdgeFilter(
            prevVar, edgeVar, rightVertexVar, edge.direction);
        firstNode->addDependency(previous);
        previous = en = lastNode;
        prevVar = rightVertexVar;

        addPathEdge(pathEdges, edgeDestinationVariable);
        addPathVertex(pathVertices, vertexDestinationVariable);
      } else {
        // Variable-length: edge.variable is a path object, so edge-document
        // RETURN projections do not apply here. Target vertex projections do.
        Variable const* vertexDestinationVariable = nullptr;
        Variable const* vertexTraversalOutputVariable = nullptr;
        bool vertexHasProjection = false;

        if (target.kind == MatchPatternElement::Kind::kVariableReference) {
          vertexDestinationVariable = target.variableReference;
          vertexTraversalOutputVariable = nullptr;
        } else {
          ADB_PROD_ASSERT(target.kind == MatchPatternElement::Kind::kVertex);
          ADB_PROD_ASSERT(target.vertex.has_value());
          vertexDestinationVariable = target.vertex->variable;
          vertexHasProjection = target.vertex->projection.has_value();
          vertexTraversalOutputVariable =
              vertexHasProjection ? _ast->variables()->createTemporaryVariable()
                                  : vertexDestinationVariable;
          if (vertexHasProjection) {
            variableScope.registerSubstitution(vertexDestinationVariable,
                                               vertexTraversalOutputVariable);
          }
        }

        auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
            prevVar, edge, target, /*edgeDocumentOutputVariable*/ nullptr,
            vertexTraversalOutputVariable, variableScope.map());

        firstNode->addDependency(previous);
        previous = en = lastNode;

        if (vertexHasProjection) {
          projections.push_back(_projections.createDocumentPatternProjection(
              vertexDestinationVariable, rightVertexVar,
              target.vertex->projection, variableScope.map()));
        }

        prevVar = rightVertexVar;

        appendTraversalPath(pathVertices, pathEdges, edge.variable);
      }
    }

    for (auto* p : projections) {
      if (p != nullptr) {
        p->addDependency(previous);
        previous = en = p;
      }
    }

    if (pathVariable != nullptr) {
      auto calcNode =
          constructPathObject(pathVariable, pathVertices, pathEdges);
      calcNode->addDependency(previous);
      previous = en = calcNode;
    }
  }

  return en;
}

}  // namespace arangodb::aql
