////////////////////////////////////////////////////////////////////////////////
/// @brief Implementation of Traversal Execution Node
///
/// @file arangod/Aql/TraversalNode.cpp
///
/// DISCLAIMER
///
/// Copyright 2010-2014 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "TraversalNode.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/SortCondition.h"
#include "Aql/Variable.h"
#include "Cluster/ClusterComm.h"
#include "Graph/BaseOptions.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/TraverserOptions.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::traverser;

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn)
    : EdgeConditionBuilder(tn->_plan->getAst()->createNodeNaryOperator(
          NODE_TYPE_OPERATOR_NARY_AND)),
      _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, arangodb::velocypack::Slice const& condition)
    : EdgeConditionBuilder(new AstNode(tn->_plan->getAst(), condition)),
      _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, TraversalEdgeConditionBuilder const* other)
    : EdgeConditionBuilder(other->_modCondition), _tn(tn) {
  _fromCondition = other->_fromCondition;
  _toCondition = other->_toCondition;
  _containsCondition = other->_containsCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildFromCondition() {
  // TODO Move computation in here.
  _fromCondition = _tn->_fromCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildToCondition() {
  // TODO Move computation in here.
  _toCondition = _tn->_toCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::toVelocyPack(
    VPackBuilder& builder, bool verbose) {
  if (_containsCondition) {
    _modCondition->removeMemberUnchecked(_modCondition->numMembers() - 1);
    _containsCondition = false;
  }
  _modCondition->toVelocyPack(builder, verbose);
}

TraversalNode::TraversalNode(ExecutionPlan* plan, size_t id,
                             TRI_vocbase_t* vocbase, AstNode const* direction,
                             AstNode const* start, AstNode const* graph,
                             std::unique_ptr<BaseOptions>& options)
    : GraphNode(plan, id, vocbase, direction, graph, options),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
      _condition(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  TRI_ASSERT(start != nullptr);

  auto ast = _plan->getAst();
  // Let us build the conditions on _from and _to. Just in case we need them.
  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::FromString.c_str(),
        StaticStrings::FromString.length());
    _fromCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, _tmpIdNode);
  }
  TRI_ASSERT(_fromCondition != nullptr);
  TRI_ASSERT(_fromCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::ToString.c_str(),
        StaticStrings::ToString.length());
    _toCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                 access, _tmpIdNode);
  }
  TRI_ASSERT(_toCondition != nullptr);
  TRI_ASSERT(_toCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  // Parse start node
  switch (start->type) {
    case NODE_TYPE_REFERENCE:
      _inVariable = static_cast<Variable*>(start->getData());
      _vertexId = "";
      break;
    case NODE_TYPE_VALUE:
      if (start->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                       "invalid start vertex. Must either be "
                                       "an _id string or an object with _id.");
      }
      _inVariable = nullptr;
      _vertexId = start->getString();
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid start vertex. Must either be an "
                                     "_id string or an object with _id.");
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
}

/// @brief Internal constructor to clone the node.
TraversalNode::TraversalNode(
    ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
    std::vector<std::unique_ptr<Collection>> const& edgeColls,
    std::vector<std::unique_ptr<Collection>> const& vertexColls,
    Variable const* inVariable, std::string const& vertexId,
    std::vector<TRI_edge_direction_e> const& directions,
    std::unique_ptr<BaseOptions>& options)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, directions, options),
      _pathOutVariable(nullptr),
      _inVariable(inVariable),
      _vertexId(vertexId),
      _condition(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {}

TraversalNode::TraversalNode(ExecutionPlan* plan,
                             arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
      _condition(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  // In Vertex
  if (base.hasKey("inVariable")) {
    _inVariable = Variable::varFromVPack(plan->getAst(), base, "inVariable");
  } else {
    VPackSlice v = base.get("vertexId");
    if (!v.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex must be a string");
    }
    _vertexId = v.copyString();
    if (_vertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex mustn't be empty");
    }
  }

  if (base.hasKey("condition")) {
    VPackSlice condition = base.get("condition");
    if (!condition.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "condition must be an object");
    }
    _condition = Condition::fromVPack(plan, condition);
  }
  auto list = base.get("conditionVariables");

  if (list.isArray()) {
    for (auto const& v : VPackArrayIterator(list)) {
      _conditionVariables.emplace(
          _plan->getAst()->variables()->createVariable(v));
    }
  }

  // Out variables
 if (base.hasKey("pathOutVariable")) {
    _pathOutVariable = Variable::varFromVPack(plan->getAst(), base, "pathOutVariable");
  }

  // Filter Condition Parts
  TRI_ASSERT(base.hasKey("fromCondition"));
  _fromCondition = new AstNode(plan->getAst(), base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  _toCondition = new AstNode(plan->getAst(), base.get("toCondition"));

  list = base.get("globalEdgeConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalEdgeConditions.emplace_back(new AstNode(plan->getAst(), cond));
    }
  }

  list = base.get("globalVertexConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalVertexConditions.emplace_back(new AstNode(plan->getAst(), cond));
    }
  }

  list = base.get("vertexConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      _vertexConditions.emplace(StringUtils::uint64(key),
                                new AstNode(plan->getAst(), cond.value));
    }
  }

  list = base.get("edgeConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      auto ecbuilder =
          std::make_unique<TraversalEdgeConditionBuilder>(this, cond.value);
      _edgeConditions.emplace(StringUtils::uint64(key), std::move(ecbuilder));
    }
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
}

TraversalNode::~TraversalNode() {
  if (_condition != nullptr) {
    delete _condition;
  }
}

int TraversalNode::checkIsOutVariable(size_t variableId) const {
  if (_vertexOutVariable != nullptr && _vertexOutVariable->id == variableId) {
    return 0;
  }
  if (_edgeOutVariable != nullptr && _edgeOutVariable->id == variableId) {
    return 1;
  }
  if (_pathOutVariable != nullptr && _pathOutVariable->id == variableId) {
    return 2;
  }
  return -1;
}

/// @brief check whether an access is inside the specified range
bool TraversalNode::isInRange(uint64_t depth, bool isEdge) const {
  auto opts = static_cast<TraverserOptions*>(options());
  if (isEdge) {
    return (depth < opts->maxDepth);
  }
  return (depth <= opts->maxDepth);
}

/// @brief check if all directions are equal
bool TraversalNode::allDirectionsEqual() const {
  if (_directions.empty()) {
    // no directions!
    return false;
  }
  size_t const n = _directions.size();
  TRI_edge_direction_e const expected = _directions[0];

  for (size_t i = 1; i < n; ++i) {
    if (_directions[i] != expected) {
      return false;
    }
  }
  return true;
}

void TraversalNode::toVelocyPackHelper(arangodb::velocypack::Builder& nodes,
                                       bool verbose) const {
  GraphNode::toVelocyPackHelper(nodes, verbose);  // call base class method
  // In variable
  if (usesInVariable()) {
    nodes.add(VPackValue("inVariable"));
    inVariable()->toVelocyPack(nodes);
  } else {
    nodes.add("vertexId", VPackValue(_vertexId));
  }

  // Condition
  if (_condition != nullptr) {
    nodes.add(VPackValue("condition"));
    _condition->toVelocyPack(nodes, verbose);
  }

  if (!_conditionVariables.empty()) {
    nodes.add(VPackValue("conditionVariables"));
    nodes.openArray();
    for (auto const& it : _conditionVariables) {
      it->toVelocyPack(nodes);
    }
    nodes.close();
  }

  // Out variables
  if (usesPathOutVariable()) {
    nodes.add(VPackValue("pathOutVariable"));
    pathOutVariable()->toVelocyPack(nodes);
  }

  // Traversal Filter Conditions

  TRI_ASSERT(_fromCondition != nullptr);
  nodes.add(VPackValue("fromCondition"));
  _fromCondition->toVelocyPack(nodes, verbose);

  TRI_ASSERT(_toCondition != nullptr);
  nodes.add(VPackValue("toCondition"));
  _toCondition->toVelocyPack(nodes, verbose);

  if (!_globalEdgeConditions.empty()) {
    nodes.add(VPackValue("globalEdgeConditions"));
    nodes.openArray();
    for (auto const& it : _globalEdgeConditions) {
      it->toVelocyPack(nodes, verbose);
    }
    nodes.close();
  }

  if (!_globalVertexConditions.empty()) {
    nodes.add(VPackValue("globalVertexConditions"));
    nodes.openArray();
    for (auto const& it : _globalVertexConditions) {
      it->toVelocyPack(nodes, verbose);
    }
    nodes.close();
  }

  if (!_vertexConditions.empty()) {
    nodes.add(VPackValue("vertexConditions"));
    nodes.openObject();
    for (auto const& it : _vertexConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, verbose);
    }
    nodes.close();
  }

  if (!_edgeConditions.empty()) {
    nodes.add(VPackValue("edgeConditions"));
    nodes.openObject();
    for (auto& it : _edgeConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, verbose);
    }
    nodes.close();
  }

  nodes.add(VPackValue("indexes"));
  _options->toVelocyPackIndexes(nodes);

  // And close it:
  nodes.close();
}

/// @brief clone ExecutionNode recursively
ExecutionNode* TraversalNode::clone(ExecutionPlan* plan, bool withDependencies,
                                    bool withProperties) const {
  TRI_ASSERT(!_optionsBuilt);
  auto oldOpts = static_cast<TraverserOptions*>(options());
  std::unique_ptr<BaseOptions> tmp =
      std::make_unique<TraverserOptions>(*oldOpts);
  auto c = new TraversalNode(plan, _id, _vocbase, _edgeColls, _vertexColls,
                             _inVariable, _vertexId, _directions, tmp);

  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable =
          plan->getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c->setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable =
          plan->getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c->setEdgeOutput(edgeOutVariable);
  }

  if (usesPathOutVariable()) {
    auto pathOutVariable = _pathOutVariable;
    if (withProperties) {
      pathOutVariable =
          plan->getAst()->variables()->createVariable(pathOutVariable);
    }
    TRI_ASSERT(pathOutVariable != nullptr);
    c->setPathOutput(pathOutVariable);
  }

  c->_conditionVariables.reserve(_conditionVariables.size());
  for (auto const& it : _conditionVariables) {
    c->_conditionVariables.emplace(it->clone());
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif

  // Temporary Filter Objects
  c->_tmpObjVariable = _tmpObjVariable;
  c->_tmpObjVarNode = _tmpObjVarNode;
  c->_tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c->_fromCondition = _fromCondition->clone(_plan->getAst());
  c->_toCondition = _toCondition->clone(_plan->getAst());
  c->_globalEdgeConditions.insert(c->_globalEdgeConditions.end(),
                                  _globalEdgeConditions.begin(),
                                  _globalEdgeConditions.end());
  c->_globalVertexConditions.insert(c->_globalVertexConditions.end(),
                                    _globalVertexConditions.begin(),
                                    _globalVertexConditions.end());

  for (auto const& it : _edgeConditions) {
    // Copy the builder
    auto ecBuilder =
        std::make_unique<TraversalEdgeConditionBuilder>(this, it.second.get());
    c->_edgeConditions.emplace(it.first, std::move(ecBuilder));
  }

  for (auto const& it : _vertexConditions) {
    c->_vertexConditions.emplace(it.first, it.second->clone(_plan->getAst()));
  }

#ifdef TRI_ENABLE_MAINTAINER_MODE
  c->checkConditionsDefined();
#endif

  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

/// @brief the cost of a traversal node
double TraversalNode::estimateCost(size_t& nrItems) const {
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  return depCost + (incoming * _options->estimateCost(nrItems));
}

void TraversalNode::prepareOptions() {
  if (_optionsBuilt) {
    return;
  }
  TRI_ASSERT(!_optionsBuilt);
  _options->setVariable(_tmpObjVariable);

  size_t numEdgeColls = _edgeColls.size();
  TraversalEdgeConditionBuilder globalEdgeConditionBuilder(this);

  for (auto& it : _globalEdgeConditions) {
    globalEdgeConditionBuilder.addConditionPart(it);
  }

  Ast* ast = _plan->getAst();

  // Compute Edge Indexes. First default indexes:
  for (size_t i = 0; i < numEdgeColls; ++i) {
    auto dir = _directions[i];
    switch (dir) {
      case TRI_EDGE_IN:
        _options->addLookupInfo(
            _plan, _edgeColls[i]->getName(), StaticStrings::ToString,
            globalEdgeConditionBuilder.getInboundCondition()->clone(ast));
        break;
      case TRI_EDGE_OUT:
        _options->addLookupInfo(
            _plan, _edgeColls[i]->getName(), StaticStrings::FromString,
            globalEdgeConditionBuilder.getOutboundCondition()->clone(ast));
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
  }

  auto opts = static_cast<TraverserOptions*>(options());
  TRI_ASSERT(opts != nullptr);
  for (auto& it : _edgeConditions) {
    uint64_t depth = it.first;
    // We probably have to adopt minDepth. We cannot fulfill a condition of
    // larger depth anyway
    auto& builder = it.second;

    for (auto& it : _globalEdgeConditions) {
      builder->addConditionPart(it);
    }

    for (size_t i = 0; i < numEdgeColls; ++i) {
      auto dir = _directions[i];
      // TODO we can optimize here. indexCondition and Expression could be
      // made non-overlapping.
      switch (dir) {
        case TRI_EDGE_IN:
          opts->addDepthLookupInfo(
              _plan, _edgeColls[i]->getName(), StaticStrings::ToString,
              builder->getInboundCondition()->clone(ast), depth);
          break;
        case TRI_EDGE_OUT:
          opts->addDepthLookupInfo(
              _plan, _edgeColls[i]->getName(), StaticStrings::FromString,
              builder->getOutboundCondition()->clone(ast), depth);
          break;
        case TRI_EDGE_ANY:
          TRI_ASSERT(false);
          break;
      }
    }
  }

  for (auto& it : _vertexConditions) {
    // We inject the base conditions as well here.
    for (auto const& jt : _globalVertexConditions) {
      it.second->addMember(jt);
    }
    opts->_vertexExpressions.emplace(it.first, new Expression(ast, it.second));
    TRI_ASSERT(!opts->_vertexExpressions[it.first]->isV8());
  }
  if (!_globalVertexConditions.empty()) {
    auto cond =
        _plan->getAst()->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    for (auto const& it : _globalVertexConditions) {
      cond->addMember(it);
    }
    opts->_baseVertexExpression = new Expression(ast, cond);
    TRI_ASSERT(!opts->_baseVertexExpression->isV8());
  }
  // If we use the path output the cache should activate document
  // caching otherwise it is not worth it.
  if (ServerState::instance()->isCoordinator()) {
    _options->activateCache(false, engines());
  } else {
    _options->activateCache(false, nullptr);
  }
  _optionsBuilt = true;
}

/// @brief remember the condition to execute for early traversal abortion.
void TraversalNode::setCondition(arangodb::aql::Condition* condition) {
  std::unordered_set<Variable const*> varsUsedByCondition;

  Ast::getReferencedVariables(condition->root(), varsUsedByCondition);

  for (auto const& oneVar : varsUsedByCondition) {
    if ((_vertexOutVariable == nullptr ||
         oneVar->id != _vertexOutVariable->id) &&
        (_edgeOutVariable == nullptr || oneVar->id != _edgeOutVariable->id) &&
        (_pathOutVariable == nullptr || oneVar->id != _pathOutVariable->id) &&
        (_inVariable == nullptr || oneVar->id != _inVariable->id)) {
      _conditionVariables.emplace(oneVar);
    }
  }

  _condition = condition;
}

void TraversalNode::registerCondition(bool isConditionOnEdge,
                                      uint64_t conditionLevel,
                                      AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    auto const& it = _edgeConditions.find(conditionLevel);
    if (it == _edgeConditions.end()) {
      auto builder = std::make_unique<TraversalEdgeConditionBuilder>(this);
      builder->addConditionPart(condition);
      _edgeConditions.emplace(conditionLevel, std::move(builder));
    } else {
      it->second->addConditionPart(condition);
    }
  } else {
    auto const& it = _vertexConditions.find(conditionLevel);
    if (it == _vertexConditions.end()) {
      auto cond =
          _plan->getAst()->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
      cond->addMember(condition);
      _vertexConditions.emplace(conditionLevel, cond);
    } else {
      it->second->addMember(condition);
    }
  }
}

void TraversalNode::registerGlobalCondition(bool isConditionOnEdge,
                                            AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    _globalEdgeConditions.emplace_back(condition);
  } else {
    _globalVertexConditions.emplace_back(condition);
  }
}

void TraversalNode::getConditionVariables(
    std::vector<Variable const*>& res) const {
  for (auto const& it : _conditionVariables) {
    if (it != _tmpObjVariable) {
      res.emplace_back(it);
    }
  }
}

#ifdef TRI_ENABLE_MAINTAINER_MODE
void TraversalNode::checkConditionsDefined() const {
  TRI_ASSERT(_tmpObjVariable != nullptr);
  TRI_ASSERT(_tmpObjVarNode != nullptr);
  TRI_ASSERT(_tmpIdNode != nullptr);

  TRI_ASSERT(_fromCondition != nullptr);
  TRI_ASSERT(_fromCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  TRI_ASSERT(_toCondition != nullptr);
  TRI_ASSERT(_toCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);
}
#endif
