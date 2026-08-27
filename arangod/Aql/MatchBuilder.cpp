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
#include "Aql/Variable.h"
#include "Aql/QueryContext.h"
#include "Basics/Exceptions.h"
#include "Graph/TraverserOptions.h"

#include <absl/strings/str_cat.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <unordered_set>
#include <utility>

namespace arangodb::aql {

MatchBuilder::MatchBuilder(ExecutionPlan& plan, Ast* ast)
    : _plan(plan), _ast(ast) {}

AstNode* MatchBuilder::createPropertyAccess(Variable const* variable,
                                            std::string_view property) {
  return _ast->createNodeAttributeAccess(_ast->createNodeReference(variable),
                                         property);
}

size_t MatchBuilder::patternEdgeCollectionCount(
    AstNode const* edgeLabelMember) {
  TRI_ASSERT(edgeLabelMember != nullptr &&
             edgeLabelMember->type == NODE_TYPE_ARRAY);
  return edgeLabelMember->numMembers();
}

AstNode const* MatchBuilder::getPatternEdgeCollection(
    AstNode const* edgeLabelMember, size_t index) {
  TRI_ASSERT(edgeLabelMember->type == NODE_TYPE_ARRAY);
  return edgeLabelMember->getMember(index);
}

AstNode* MatchBuilder::buildPatternEdgeCollectionList(
    AstNode const* edgeLabelMember) {
  auto* edgeCollectionList = _ast->createNodeArray();
  size_t const n = patternEdgeCollectionCount(edgeLabelMember);
  for (size_t i = 0; i < n; ++i) {
    edgeCollectionList->addMember(getPatternEdgeCollection(edgeLabelMember, i));
  }
  return edgeCollectionList;
}

std::tuple<CalculationNode*, FilterNode*> MatchBuilder::createPropertiesFilter(
    Variable const* variable, AstNode* properties,
    AstNode* additionalExpression) {
  AstNode* root = additionalExpression->type == NODE_TYPE_NOP
                      ? nullptr
                      : additionalExpression;
  ADB_PROD_ASSERT(properties->type == NODE_TYPE_OBJECT ||
                  properties->type == NODE_TYPE_NOP);

  for (auto member : properties->getMemberList()) {
    ADB_PROD_ASSERT(member->type == NODE_TYPE_OBJECT_ELEMENT);

    auto key = member->getStringView();
    auto value = member->getMember(0);

    auto access = createPropertyAccess(variable, key);
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
    AstNode const* member, Variable const* fullDocumentVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  auto collectionName = member->getMember(1)->getString();
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

  auto additionalFilter = Ast::replaceVariables(member->getMember(3), subst);

  auto [firstNode, lastNode] = createPropertiesFilter(
      fullDocumentVariable, member->getMember(2), additionalFilter);
  firstNode->addDependency(enumCollection);
  return std::make_tuple(enumCollection, lastNode, fullDocumentVariable);
}

ExecutionNode* MatchBuilder::createPatternProjection(
    AstNode const* member, Variable const* fullDocumentVar,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  auto variable = static_cast<Variable const*>(member->getMember(0)->getData());

  // Vertices store projections at member 4 and edges store them at member 6
  // (appended after direction/range by createPatternEdge).
  AstNode const* projections = nullptr;
  bool const isEdge = member->type == NODE_TYPE_PATTERN_EDGE;
  if (isEdge) {
    ADB_PROD_ASSERT(member->numMembers() > 6)
        << "pattern edge missing projection member";
    projections = member->getMember(6);
  } else {
    projections = member->getMember(4);
  }

  if (projections->type != NODE_TYPE_NOP) {
    auto* root = _ast->createNodeObject();

    auto* ref = _ast->createNodeReference(fullDocumentVar);

    auto isSystemAttribute = [&](std::string_view name) {
      return name == "_id" || (isEdge && (name == "_from" || name == "_to"));
    };

    // createNodeObjectElement stores the raw pointer from string_view. keys
    // must outlive the AST (register into Ast resources).
    auto registerKey = [&](std::string_view key) -> std::string_view {
      char const* p = _ast->resources().registerString(key);
      return {p, key.size()};
    };

    // Find an existing nested OBJECT member named `key`, or create one.
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
      object->addMember(
          _ast->createNodeObjectElement(registerKey(key), nested));
      return nested;
    };

    // Insert valueExpr at path segments under object, rebuilding hierarchy
    // (e.g. ["profile","name"] → { profile: { name: valueExpr } }).
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

    // Implicit system attributes required for graph topology.
    addProjectedAttribute({"_id"});
    if (isEdge) {
      addProjectedAttribute({"_from"});
      addProjectedAttribute({"_to"});
    }

    // Collect bare keep paths and alias items, bare keeps use PATH arrays
    // (unquoted dotted) or VALUE strings (quoted literal keys).
    std::vector<std::vector<std::string>> keepPaths;
    struct AliasItem {
      std::string_view name;
      AstNode* expr;
    };
    std::vector<AliasItem> aliases;

    auto extractPath = [](AstNode const* item) -> std::vector<std::string> {
      std::vector<std::string> path;
      if (item->type == NODE_TYPE_ARRAY) {
        path.reserve(item->numMembers());
        for (size_t j = 0; j < item->numMembers(); ++j) {
          AstNode const* part = item->getMemberUnchecked(j);
          TRI_ASSERT(part->isStringValue());
          path.emplace_back(part->getString());
        }
      } else {
        // Quoted literal keep: single top-level key (dots not hierarchy).
        TRI_ASSERT(item->isStringValue());
        path.emplace_back(item->getString());
      }
      return path;
    };

    for (auto i = size_t{0}; i < projections->numMembers(); ++i) {
      AstNode* item = projections->getMemberUnchecked(i);
      if (item->type == NODE_TYPE_OBJECT_ELEMENT) {
        aliases.push_back(AliasItem{item->getStringView(), item->getMember(0)});
      } else {
        auto path = extractPath(item);
        // Skip any RETURN whose first segment is an implicit system attribute
        // (bare `_id` and nested `_id.foo` / `_from.x`), so we never
        // overwrite the scalar system attrs with a nested object of the same
        // key.
        if (!path.empty() && isSystemAttribute(path[0])) {
          continue;
        }
        keepPaths.push_back(std::move(path));
      }
    }

    // Drop paths that are duplicates or have a shorter kept prefix
    // (same policy as Projections::handleSharedPrefixes).
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

    // Top-level keys already claimed by bare keeps (and implicit system
    // attrs). Alias names must not collide with these, or with each other.
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
      root->addMember(_ast->createNodeObjectElement(alias.name, expr));
    }

    auto* calc = _plan.createNode<CalculationNode>(
        &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
        variable);
    return calc;
  } else {
    auto* root = _ast->createNodeReference(fullDocumentVar);
    auto* calc = _plan.createNode<CalculationNode>(
        &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
        variable);
    return calc;
  }
}

std::tuple<ExecutionNode*, ExecutionNode*, Variable const*>
MatchBuilder::createPatternEdgeEnumerateAccess(
    AstNode const* edge, Variable const* outputVariable,
    std::unordered_map<VariableId, Variable const*> const& subst) {
  auto const* collectionNode = getPatternEdgeCollection(edge->getMember(1), 0);
  auto collectionName = collectionNode->getString();
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
  auto additionalFilter = Ast::replaceVariables(edge->getMember(3), subst);
  auto [firstNode, lastNode] = createPropertiesFilter(
      outputVariable, edge->getMember(2), additionalFilter);
  firstNode->addDependency(enumCollection);
  return std::make_tuple(enumCollection, lastNode, outputVariable);
}

std::tuple<CalculationNode*, FilterNode*> MatchBuilder::createVertexEdgeFilter(
    Variable const* leftVertex, Variable const* edge,
    Variable const* rightVertex, int direction) {
  AstNode* root =
      nullptr;  //_ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_OR);

  if (direction & 2) {
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
  if (direction & 1) {
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
      auto orNode = _ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_OR,
                                                   root, andNode);
      root = orNode;
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
MatchBuilder::createTraversalForPattern(Variable const* startNodeVar,  //
                                        AstNode const* edge,
                                        /* edge part of the pattern */  //
                                        AstNode const* node) {
  auto const* patternEdgeOutputVariable =
      static_cast<Variable const*>(edge->getMember(0)->getData());
  auto const* patternEdgeCollections = edge->getMember(1);

  aql::QueryContext& query = _ast->query();
  auto options = std::make_unique<traverser::TraverserOptions>(query);
  auto range = edge->getMember(5);
  if (range->type == NODE_TYPE_NOP) {
    options->minDepth = 1;
    options->maxDepth = 1;
  } else {
    options->minDepth = range->getMember(0)->getIntValue();
    options->maxDepth = range->getMember(1)->getIntValue();
  }

  auto dirNode = _ast->createNodeValueInt(std::invoke(
      [](int64_t d) {
        switch (d) {
          case 1:
            return 1;
          case 2:
            return 2;
          case 3:
            return 0;
          default:
            THROW_ARANGO_EXCEPTION_MESSAGE(
                TRI_ERROR_INTERNAL, "invalid direction for match expression");
        }
      },
      edge->getMember(4)->getIntValue()));

  auto* startNode = _ast->createNodeReference(startNodeVar);

  auto* edgeCollectionList =
      buildPatternEdgeCollectionList(patternEdgeCollections);
  auto* graphNode = _ast->createNodeCollectionList(edgeCollectionList,
                                                   _ast->query().resolver());

  auto* traversal = _plan.createNode<TraversalNode>(
      &_plan /* plan */,                       //
      _plan.nextId() /* id */,                 //
      &_ast->query().vocbase() /* vocbase */,  //
      dirNode /* direction */,                 //
      startNode, /* start */                   //
      graphNode, /* graph */                   //
      nullptr, /* prune expression */          //
      std::move(options));

  bool const fixedDepth = range->type == NODE_TYPE_NOP;
  if (fixedDepth) {
    // fixed-depth multi-edge patterns use a 1..1 traversal over all listed
    // edge collections and expose the edge document in the pattern variable
    traversal->setEdgeOutput(patternEdgeOutputVariable);
  } else {
    traversal->setPathOutput(
        patternEdgeOutputVariable); /* note that it is intentional to
                                    output the path segment that is
                                    output by the traversal into the
                                    edge variable in the pattern; see
                                    transformations below */
    auto traversalEdgeOutputVar = _ast->variables()->createTemporaryVariable();
    traversal->setEdgeOutput(traversalEdgeOutputVar);
  }

  switch (node->type) {
    case NODE_TYPE_REFERENCE: {
      /*
        FOR w IN ovc
          MATCH (v:vc) -[ e:ec * 2..3 ]-> (w)
            RETURN [v, e, w]

        FOR w IN ovc
          FOR v IN vc
            FOR #3,_,e IN 2..3 OUTBOUND v ec
              FILTER #3._id == w._id
              RETURN [v, e, w]
       */

      auto const* traversalVertexOutputVar =
          _ast->variables()->createTemporaryVariable();
      traversal->setVertexOutput(traversalVertexOutputVar);
      auto rightVertexVar = static_cast<Variable*>(node->getData());

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

    } break;
    case NODE_TYPE_PATTERN_NODE_PATTERN: {
      /*
         MATCH (v:vc) -[ e:ec * 2..3 ]-> (w:ovc)
          RETURN [v, e, w]

        FOR v IN vc
          FOR w,_,e IN 2..3 OUTBOUND v ec
            FILTER IS_SAME_COLLECTION(w._id, "ovc")
            RETURN [v, e, w]
      */

      auto traversalVertexOutputVar =
          static_cast<Variable const*>(node->getMember(0)->getData());
      traversal->setVertexOutput(traversalVertexOutputVar);

      auto traversalVertexOutputId =
          createPropertyAccess(traversalVertexOutputVar, "_id");
      auto vertexCollection = node->getMember(1);
      auto vertexCollectionName = vertexCollection->getStringView();

      auto args = _ast->createNodeArray();
      args->addMember(traversalVertexOutputId);
      args->addMember(_ast->createNodeValueString(
          vertexCollectionName.data(), vertexCollectionName.length()));

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

      return std::make_tuple(traversal, filter, traversalVertexOutputVar);
    } break;
    default: {
      // Throw
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unexpected match expression member");
      return std::make_tuple(nullptr, nullptr, nullptr);
    }
  }
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

  CalculationNode* calc = _plan.createNode<CalculationNode>(
      &_plan, _plan.nextId(), std::make_unique<Expression>(_ast, root),
      outVariable);

  return calc;
}

ExecutionNode* MatchBuilder::build(ExecutionNode* previous,
                                   AstNode const* matchNode) {
  auto en = previous;
  auto nMembers = matchNode->numMembers();

  for (size_t i = 0; i < nMembers; ++i) {
    auto matchExpr = matchNode->getMemberUnchecked(i);

    Variable const* prevVar = nullptr;
    Variable const* pathVariable = nullptr;
    std::vector<AstNode const*> pathVertices;
    std::vector<AstNode const*> pathEdges;
    std::vector<ExecutionNode*> projections;

    std::unordered_map<VariableId, Variable const*> variableSubstitutions;

    ADB_PROD_ASSERT(matchExpr->type == NODE_TYPE_PATTERN_MATCH_EXPRESSION);

    for (size_t j = 0; j < matchExpr->numMembers(); ++j) {
      auto member = matchExpr->getMemberUnchecked(j);

      if (member->type == NODE_TYPE_PATTERN_NODE_PATTERN) {
        // generate code like
        // FOR <outvariable> IN <collection>
        //  FILTER <outvariable>.property1 == xxx && ....
        ExecutionNode* lastNode;
        auto destinationVariable =
            static_cast<Variable const*>(member->getMember(0)->getData());

        // Without an inline projection we enumerate directly into the
        // user-facing variable (as the non-projection lowering does), so that
        // downstream rules and outputs observe the expected variable. With a
        // projection we enumerate into a temporary full-document variable and
        // copy the projected attributes into the user variable.
        bool const hasProjection = member->getMember(4)->type != NODE_TYPE_NOP;
        Variable const* enumOutputVariable =
            hasProjection ? _ast->variables()->createTemporaryVariable()
                          : destinationVariable;
        if (hasProjection) {
          variableSubstitutions.emplace(destinationVariable->id,
                                        enumOutputVariable);
        }

        std::tie(en, lastNode, prevVar) = createCollectionAccess(
            member, enumOutputVariable, variableSubstitutions);
        en->addDependency(previous);
        previous = en = lastNode;

        pathVertices.push_back(_ast->createNodeReference(destinationVariable));

        if (hasProjection) {
          auto projection =
              createPatternProjection(member, prevVar, variableSubstitutions);
          projections.push_back(projection);
        }
      } else if (member->type == NODE_TYPE_PATTERN_PATH_VARIABLE) {
        pathVariable = static_cast<Variable const*>(member->getData());
      } else if (member->type == NODE_TYPE_REFERENCE) {
        prevVar = static_cast<Variable*>(member->getData());
        if (auto it = variableSubstitutions.find(prevVar->id);
            it != std::end(variableSubstitutions)) {
          prevVar = it->second;
        }
        pathVertices.push_back(_ast->createNodeReference(prevVar));
      } else if (member->type == NODE_TYPE_PATTERN_SEGMENT) {
        // generate code like
        //  FOR <outvar> IN <edge>
        //    FILTER <outvar>._from == <prevVar>._id
        auto edge = member->getMember(0);
        auto node = member->getMember(1);
        ADB_PROD_ASSERT(prevVar != nullptr);

        if (edge->getMember(5)->type == NODE_TYPE_NOP &&
            patternEdgeCollectionCount(edge->getMember(1)) > 1) {
          auto [firstNode, lastNode, rightVertexVar] =
              createTraversalForPattern(prevVar, edge, node);

          firstNode->addDependency(previous);
          previous = en = lastNode;

          auto const* edgeVar =
              static_cast<Variable const*>(edge->getMember(0)->getData());
          if (edge->getMember(2)->type != NODE_TYPE_NOP ||
              edge->getMember(3)->type != NODE_TYPE_NOP) {
            auto [propCalc, propFilter] = createPropertiesFilter(
                edgeVar, edge->getMember(2), edge->getMember(3));
            propCalc->addDependency(previous);
            previous = en = propFilter;
          }

          prevVar = rightVertexVar;

          pathEdges.push_back(_ast->createNodeReference(edgeVar));
          pathVertices.push_back(_ast->createNodeReference(prevVar));
        } else if (edge->getMember(5)->type == NODE_TYPE_NOP) {
          ExecutionNode* lastNodeFilter;
          Variable const* edgeVar;

          auto edgeDestinationVariable =
              static_cast<Variable const*>(edge->getMember(0)->getData());

          // Same reasoning as for vertices: only introduce a temporary
          // full-document variable plus a projection copy when an inline
          // projection is actually requested. Otherwise enumerate straight
          // into the user-facing edge variable.
          bool const edgeHasProjection =
              edge->getMember(6)->type != NODE_TYPE_NOP;
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
            auto edgeProjection = createPatternProjection(
                edge, edgeEnumOutputVariable, variableSubstitutions);
            projections.push_back(edgeProjection);
          }

          Variable const* rightVertexVar;
          Variable const* vertexDestinationVariable = nullptr;

          if (node->type == NODE_TYPE_REFERENCE) {
            // todo: possibly replace?
            rightVertexVar = static_cast<Variable*>(node->getData());
            vertexDestinationVariable = rightVertexVar;
          } else {
            ADB_PROD_ASSERT(node->type == NODE_TYPE_PATTERN_NODE_PATTERN)
                << member->type;

            vertexDestinationVariable =
                static_cast<Variable const*>(node->getMember(0)->getData());

            bool const vertexHasProjection =
                node->getMember(4)->type != NODE_TYPE_NOP;
            Variable const* vertexEnumOutputVariable =
                vertexHasProjection
                    ? _ast->variables()->createTemporaryVariable()
                    : vertexDestinationVariable;
            if (vertexHasProjection) {
              variableSubstitutions.emplace(vertexDestinationVariable->id,
                                            vertexEnumOutputVariable);
            }

            std::tie(en, lastNodeFilter, rightVertexVar) =
                createCollectionAccess(node, vertexEnumOutputVariable,
                                       variableSubstitutions);
            en->addDependency(previous);

            if (vertexHasProjection) {
              auto projection = createPatternProjection(node, rightVertexVar,
                                                        variableSubstitutions);
              projections.push_back(projection);
            }

            previous = en = lastNodeFilter;
          }

          auto [firstNode, lastNode] =
              createVertexEdgeFilter(prevVar, edgeVar, rightVertexVar,
                                     edge->getMember(4)->getIntValue());
          firstNode->addDependency(previous);
          previous = en = lastNode;
          prevVar = rightVertexVar;

          pathEdges.push_back(
              _ast->createNodeReference(edgeDestinationVariable));

          pathVertices.push_back(
              _ast->createNodeReference(vertexDestinationVariable));
        } else {
          auto [firstNode, lastNode, rightVertexVar] =
              createTraversalForPattern(prevVar,  // start node
                                        edge,     // edge part of the pattern
                                        node      // node part of the pattern
              );

          firstNode->addDependency(previous);
          previous = en = lastNode;
          prevVar = rightVertexVar;

          pathEdges.push_back(
              _ast->createNodeArraySplice(_ast->createNodeAttributeAccess(
                  _ast->createNodeReference(static_cast<Variable const*>(
                      edge->getMember(0)->getData())),
                  "edges")));

          // The path output of the traversal contains the start vertex.
          pathVertices.pop_back();
          pathVertices.push_back(
              _ast->createNodeArraySplice(_ast->createNodeAttributeAccess(
                  _ast->createNodeReference(static_cast<Variable const*>(
                      edge->getMember(0)->getData())),
                  "vertices")));
        }

      } else {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unexpected match expression member");
      }
    }

    // insert projections
    for (auto&& p : projections) {
      // TODO don't insert into projections if nullptr?
      if (p != nullptr) {
        p->addDependency(previous);
        previous = en = p;
      }
    }

    // produce path variable if requested
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
