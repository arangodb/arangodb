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
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CalculationNode.h"
#include "Aql/ExecutionNode/EnumerateCollectionNode.h"
#include "Aql/ExecutionNode/FilterNode.h"
#include "Aql/ExecutionNode/TraversalNode.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexHint.h"
#include "Aql/MatchPatternNormalizer.h"
#include "Aql/QueryContext.h"
#include "Aql/Variable.h"
#include "Basics/Exceptions.h"
#include "Graph/TraverserOptions.h"
#include "VocBase/AccessMode.h"

#include <absl/strings/str_cat.h>

#include <algorithm>
#include <limits>
#include <memory>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {
namespace {

std::string requireCollectionName(MatchDataSource const& ds) {
  if (ds.kind() != MatchDataSource::Kind::kCollection) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "MATCH planning requires resolved collection names; unresolved "
        "collection bind parameters are not supported at plan time");
  }
  return std::string(ds.name());
}

int directionFilterBits(MatchEdgeDirection direction) {
  switch (direction) {
    case MatchEdgeDirection::kInbound:
      return 1;
    case MatchEdgeDirection::kOutbound:
      return 2;
    case MatchEdgeDirection::kAny:
      return 3;
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                 "invalid direction for match expression");
}

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
    : _plan(plan), _ast(ast) {}

AstNode* MatchBuilder::createPropertyAccess(Variable const* variable,
                                            std::string_view property) {
  char const* registered = _ast->resources().registerString(property);
  return _ast->createNodeAttributeAccess(
      _ast->createNodeReference(variable),
      std::string_view(registered, property.size()));
}

AstNode* MatchBuilder::buildEdgeCollectionList(NormalizedEdge const& edge) {
  auto* edgeCollectionList = _ast->createNodeArray();
  if (!edge.collectionAstNodes.empty()) {
    for (AstNode const* collectionNode : edge.collectionAstNodes) {
      edgeCollectionList->addMember(collectionNode);
    }
    return edgeCollectionList;
  }

  for (auto const& ds : edge.collections) {
    auto name = requireCollectionName(ds);
    edgeCollectionList->addMember(_ast->createNodeCollection(
        _ast->query().resolver(), name, AccessMode::Type::READ));
  }
  return edgeCollectionList;
}

std::tuple<CalculationNode*, FilterNode*> MatchBuilder::createPropertiesFilter(
    Variable const* variable,
    std::vector<MatchPropertyConstraint> const& properties,
    std::optional<MatchExpressionRef> const& additionalFilter,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  AstNode* root = nullptr;
  if (additionalFilter.has_value()) {
    root = Ast::replaceVariables(const_cast<AstNode*>(additionalFilter->node),
                                 subst);
  }

  for (auto const& property : properties) {
    auto access = createPropertyAccess(variable, property.key);
    auto value =
        Ast::replaceVariables(const_cast<AstNode*>(property.value.node), subst);
    auto operatorEq = _ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, value);
    if (root) {
      root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, root,
                                            operatorEq);
    } else {
      root = operatorEq;
    }
  }

  if (root == nullptr) {
    root = _ast->createNodeValueBool(true);
  }

  Variable const* filterVar = _ast->variables()->createTemporaryVariable();
  CalculationNode* calc = _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      filterVar);
  FilterNode* filter =
      _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
  filter->addDependency(calc);
  return std::make_tuple(calc, filter);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
MatchBuilder::createCollectionAccess(
    NormalizedVertex const& vertex, Variable const* fullDocumentVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  auto collectionName = requireCollectionName(vertex.collection);
  auto& collections = _ast->query().collections();
  auto collection = collections.get(collectionName);
  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no collection for EnumerateCollection");
  }
  IndexHint hint(_ast->query(), _ast->createNodeNop(),
                 IndexHint::FromCollectionOperation{});
  auto enumCollection = _plan.createNode<EnumerateCollectionNode>(
      &_plan, _plan.nextId(), collection, fullDocumentVariable, false,
      std::move(hint));

  auto [firstNode, lastNode] = createPropertiesFilter(
      fullDocumentVariable, vertex.properties, vertex.filter, subst);
  firstNode->addDependency(enumCollection);
  return std::make_tuple(enumCollection, lastNode, fullDocumentVariable);
}

ExecutionNode* MatchBuilder::createPatternProjection(
    Variable const* destinationVariable, Variable const* fullDocumentVar,
    std::optional<MatchProjection> const& projectionOpt, bool isEdge,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  if (!projectionOpt.has_value()) {
    auto* root = _ast->createNodeReference(fullDocumentVar);
    return _plan.createNode<CalculationNode>(
        &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
        destinationVariable);
  }

  auto const& projection = *projectionOpt;
  auto* root = _ast->createNodeObject();
  auto* ref = _ast->createNodeReference(fullDocumentVar);

  auto isSystemAttribute = [&](std::string_view name) {
    return name == "_id" || (isEdge && (name == "_from" || name == "_to"));
  };

  auto registerKey = [&](std::string_view key) -> std::string_view {
    char const* p = _ast->resources().registerString(key);
    return {p, key.size()};
  };

  auto findOrCreateNestedObject = [&](AstNode* object,
                                      std::string_view key) -> AstNode* {
    for (size_t i = 0; i < object->numMembers(); ++i) {
      AstNode* elt = object->getMemberUnchecked(i);
      if (elt->type == NODE_TYPE_OBJECT_ELEMENT &&
          elt->getStringView() == key &&
          elt->getMember(0)->type == NODE_TYPE_OBJECT) {
        return elt->getMember(0);
      }
    }
    auto* nested = _ast->createNodeObject();
    object->addMember(_ast->createNodeObjectElement(registerKey(key), nested));
    return nested;
  };

  auto insertNestedPath = [&](AstNode* object,
                              std::vector<std::string> const& path,
                              AstNode* valueExpr) {
    TRI_ASSERT(!path.empty());
    AstNode* cursor = object;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
      cursor = findOrCreateNestedObject(cursor, path[i]);
    }
    cursor->addMember(
        _ast->createNodeObjectElement(registerKey(path.back()), valueExpr));
  };

  auto addProjectedAttribute = [&](std::vector<std::string> const& path) {
    TRI_ASSERT(!path.empty());
    auto* attrAccess = _ast->createNodeAttributeAccess(ref, path);
    insertNestedPath(root, path, attrAccess);
  };

  addProjectedAttribute({"_id"});
  if (isEdge) {
    addProjectedAttribute({"_from"});
    addProjectedAttribute({"_to"});
  }

  std::vector<std::vector<std::string>> keepPaths;
  struct AliasItem {
    std::string_view name;
    AstNode* expr;
  };
  std::vector<AliasItem> aliases;

  for (auto const& item : projection.items) {
    if (item.kind == MatchProjectionItem::Kind::kAlias) {
      aliases.push_back(
          AliasItem{item.name, const_cast<AstNode*>(item.expression.node)});
    } else {
      TRI_ASSERT(!item.path.empty());
      if (isSystemAttribute(item.path[0])) {
        continue;
      }
      keepPaths.push_back(item.path);
    }
  }

  // Drop paths that are duplicates or have a shorter kept prefix
  {
    std::sort(keepPaths.begin(), keepPaths.end());
    keepPaths.erase(std::unique(keepPaths.begin(), keepPaths.end()),
                    keepPaths.end());
    std::vector<std::vector<std::string>> filtered;
    filtered.reserve(keepPaths.size());
    for (auto const& path : keepPaths) {
      bool covered = false;
      for (auto const& kept : filtered) {
        if (kept.size() <= path.size() &&
            std::equal(kept.begin(), kept.end(), path.begin())) {
          covered = true;
          break;
        }
      }
      if (!covered) {
        filtered.push_back(path);
      }
    }
    keepPaths = std::move(filtered);
  }

  std::unordered_set<std::string_view> usedTopLevelKeys;
  usedTopLevelKeys.emplace("_id");
  if (isEdge) {
    usedTopLevelKeys.emplace("_from");
    usedTopLevelKeys.emplace("_to");
  }
  for (auto const& path : keepPaths) {
    TRI_ASSERT(!path.empty());
    usedTopLevelKeys.emplace(path[0]);
  }

  for (auto const& path : keepPaths) {
    addProjectedAttribute(path);
  }

  for (auto const& alias : aliases) {
    if (isSystemAttribute(alias.name)) {
      continue;
    }
    if (!usedTopLevelKeys.emplace(alias.name).second) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          absl::StrCat("duplicate projection attribute name '", alias.name,
                       "'"));
    }

    // alias = expression: evaluate in normal query scope (explicit
    // variable references required, e.g. v.profile.first_name).
    AstNode* expr = Ast::replaceVariables(alias.expr, subst);
    root->addMember(
        _ast->createNodeObjectElement(registerKey(alias.name), expr));
  }

  return _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      destinationVariable);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
MatchBuilder::createPatternEdgeEnumerateAccess(
    NormalizedEdge const& edge, Variable const* outputVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  ADB_PROD_ASSERT(!edge.collections.empty());
  auto collectionName = requireCollectionName(edge.collections.front());
  auto& collections = _ast->query().collections();
  auto collection = collections.get(collectionName);
  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "no collection for EnumerateCollection");
  }
  IndexHint hint(_ast->query(), _ast->createNodeNop(),
                 IndexHint::FromCollectionOperation{});
  auto enumCollection = _plan.createNode<EnumerateCollectionNode>(
      &_plan, _plan.nextId(), collection, outputVariable, false,
      std::move(hint));
  auto [firstNode, lastNode] = createPropertiesFilter(
      outputVariable, edge.properties, edge.filter, subst);
  firstNode->addDependency(enumCollection);
  return std::make_tuple(enumCollection, lastNode, outputVariable);
}

std::tuple<CalculationNode*, FilterNode*> MatchBuilder::createVertexEdgeFilter(
    Variable const* leftVertex, Variable const* edge,
    Variable const* rightVertex, MatchEdgeDirection direction) {
  AstNode* root = nullptr;
  int const bits = directionFilterBits(direction);

  if (bits & 2) {
    auto leftVertexId = createPropertyAccess(leftVertex, "_id");
    auto rightVertexId = createPropertyAccess(rightVertex, "_id");
    auto edgeFrom = createPropertyAccess(edge, "_from");
    auto edgeTo = createPropertyAccess(edge, "_to");
    auto first = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                leftVertexId, edgeFrom);
    auto second = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 edgeTo, rightVertexId);
    root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND, first,
                                          second);
  }
  if (bits & 1) {
    auto leftVertexId = createPropertyAccess(leftVertex, "_id");
    auto rightVertexId = createPropertyAccess(rightVertex, "_id");
    auto edgeFrom = createPropertyAccess(edge, "_from");
    auto edgeTo = createPropertyAccess(edge, "_to");
    auto first = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                leftVertexId, edgeTo);
    auto second = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 edgeFrom, rightVertexId);
    auto andNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_AND,
                                                  first, second);
    if (root) {
      root = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR, root,
                                            andNode);
    } else {
      root = andNode;
    }
  }

  Variable const* filterVar = _ast->variables()->createTemporaryVariable();
  CalculationNode* calc = _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      filterVar);
  FilterNode* filter =
      _plan.createNode<FilterNode>(&_plan, _plan.nextId(), filterVar);
  filter->addDependency(calc);
  return std::make_tuple(calc, filter);
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
MatchBuilder::createTraversalForPattern(
    Variable const* startNodeVar, NormalizedEdge const& edge,
    MatchPatternElement const& target,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  auto const* patternEdgeOutputVariable = edge.variable;

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
  auto* edgeCollectionList = buildEdgeCollectionList(edge);
  auto* graphNode = _ast->createNodeCollectionList(edgeCollectionList,
                                                   _ast->query().resolver());

  auto* traversal = _plan.createNode<TraversalNode>(
      &_plan, _plan.nextId(), &_ast->query().vocbase(), dirNode, startNode,
      graphNode, nullptr, std::move(options));

  bool const fixedDepth = edge.range.isDefaultFixedOne();
  if (fixedDepth) {
    traversal->setEdgeOutput(patternEdgeOutputVariable);
  } else {
    traversal->setPathOutput(patternEdgeOutputVariable);
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
          createPropertyAccess(traversalVertexOutputVar, "_id");
      auto rightVertexId = createPropertyAccess(rightVertexVar, "_id");
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
      auto const& vertex = *target.vertex;

      auto traversalVertexOutputVar = vertex.variable;
      traversal->setVertexOutput(traversalVertexOutputVar);

      auto traversalVertexOutputId =
          createPropertyAccess(traversalVertexOutputVar, "_id");
      auto vertexCollectionName = requireCollectionName(vertex.collection);
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

      ExecutionNode* lastNode = filter;
      if (!vertex.properties.empty() || vertex.filter.has_value()) {
        auto [propCalc, propFilter] = createPropertiesFilter(
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

    std::unordered_map<VariableId, Variable const*> variableSubstitutions;

    auto const handleStartVertex = [&](NormalizedVertex const& vertex) {
      auto destinationVariable = vertex.variable;
      bool const hasProjection = vertex.projection.has_value();
      Variable const* enumOutputVariable =
          hasProjection ? _ast->variables()->createTemporaryVariable()
                        : destinationVariable;
      if (hasProjection) {
        variableSubstitutions.emplace(destinationVariable->id,
                                      enumOutputVariable);
      }

      ExecutionNode* lastNode;
      std::tie(en, lastNode, prevVar) = createCollectionAccess(
          vertex, enumOutputVariable, variableSubstitutions);
      en->addDependency(previous);
      previous = en = lastNode;

      addPathVertex(pathVertices, destinationVariable);

      if (hasProjection) {
        projections.push_back(createPatternProjection(
            destinationVariable, prevVar, vertex.projection, false,
            variableSubstitutions));
      }
    };

    if (pattern.start.kind == MatchPatternElement::Kind::kVertex) {
      ADB_PROD_ASSERT(pattern.start.vertex.has_value());
      handleStartVertex(*pattern.start.vertex);
    } else {
      ADB_PROD_ASSERT(pattern.start.kind ==
                      MatchPatternElement::Kind::kVariableReference);
      prevVar = pattern.start.variableReference;
      if (auto it = variableSubstitutions.find(prevVar->id);
          it != std::end(variableSubstitutions)) {
        prevVar = it->second;
      }
      addPathVertex(pathVertices, prevVar);
    }

    for (auto const& segment : pattern.segments) {
      auto const& edge = segment.edge;
      auto const& target = segment.target;
      ADB_PROD_ASSERT(prevVar != nullptr);

      if (edge.range.isDefaultFixedOne() && edge.collections.size() > 1) {
        auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
            prevVar, edge, target, variableSubstitutions);

        firstNode->addDependency(previous);
        previous = en = lastNode;

        auto const* edgeVar = edge.variable;
        if (!edge.properties.empty() || edge.filter.has_value()) {
          auto [propCalc, propFilter] = createPropertiesFilter(
              edgeVar, edge.properties, edge.filter, variableSubstitutions);
          propCalc->addDependency(previous);
          previous = en = propFilter;
        }

        prevVar = rightVertexVar;
        addPathEdge(pathEdges, edgeVar);
        addPathVertex(pathVertices, prevVar);
      } else if (edge.range.isDefaultFixedOne()) {
        ExecutionNode* lastNodeFilter;
        Variable const* edgeVar;

        auto edgeDestinationVariable = edge.variable;
        bool const edgeHasProjection = edge.projection.has_value();
        Variable const* edgeEnumOutputVariable =
            edgeHasProjection ? _ast->variables()->createTemporaryVariable()
                              : edgeDestinationVariable;
        if (edgeHasProjection) {
          variableSubstitutions.emplace(edgeDestinationVariable->id,
                                        edgeEnumOutputVariable);
        }

        std::tie(en, lastNodeFilter, edgeVar) =
            createPatternEdgeEnumerateAccess(edge, edgeEnumOutputVariable,
                                             variableSubstitutions);
        en->addDependency(previous);
        previous = en = lastNodeFilter;

        if (edgeHasProjection) {
          projections.push_back(createPatternProjection(
              edgeDestinationVariable, edgeEnumOutputVariable, edge.projection,
              true, variableSubstitutions));
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
            variableSubstitutions.emplace(vertexDestinationVariable->id,
                                          vertexEnumOutputVariable);
          }

          std::tie(en, lastNodeFilter, rightVertexVar) = createCollectionAccess(
              *target.vertex, vertexEnumOutputVariable, variableSubstitutions);
          en->addDependency(previous);

          if (vertexHasProjection) {
            projections.push_back(createPatternProjection(
                vertexDestinationVariable, rightVertexVar,
                target.vertex->projection, false, variableSubstitutions));
          }

          previous = en = lastNodeFilter;
        }

        auto [firstNode, lastNode] = createVertexEdgeFilter(
            prevVar, edgeVar, rightVertexVar, edge.direction);
        firstNode->addDependency(previous);
        previous = en = lastNode;
        prevVar = rightVertexVar;

        addPathEdge(pathEdges, edgeDestinationVariable);
        addPathVertex(pathVertices, vertexDestinationVariable);
      } else {
        auto [firstNode, lastNode, rightVertexVar] = createTraversalForPattern(
            prevVar, edge, target, variableSubstitutions);

        firstNode->addDependency(previous);
        previous = en = lastNode;
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
