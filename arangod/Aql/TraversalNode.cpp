////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/GraphNode.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortCondition.h"
#include "Aql/TraversalExecutor.h"
#include "Aql/Variable.h"
#include "Basics/StringUtils.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ClusterTraverser.h"
#include "Graph/Graph.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Cluster/SmartGraphTraverser.h"
#endif
#include "Graph/BaseOptions.h"
#include "Graph/SingleServerTraverser.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ticks.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;
using namespace arangodb::graph;
using namespace arangodb::traverser;

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(TraversalNode const* tn)
    : EdgeConditionBuilder(tn->_plan->getAst()->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND)),
      _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, arangodb::velocypack::Slice const& condition)
    : EdgeConditionBuilder(tn->_plan->getAst()->createNode(condition)), _tn(tn) {}

TraversalNode::TraversalEdgeConditionBuilder::TraversalEdgeConditionBuilder(
    TraversalNode const* tn, TraversalEdgeConditionBuilder const* other)
   : EdgeConditionBuilder(tn->_plan->getAst(), *other), _tn(tn) {
  _fromCondition = other->_fromCondition;
  _toCondition = other->_toCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildFromCondition() {
  // TODO Move computation in here.
  _fromCondition = _tn->_fromCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::buildToCondition() {
  // TODO Move computation in here.
  _toCondition = _tn->_toCondition;
}

void TraversalNode::TraversalEdgeConditionBuilder::toVelocyPack(VPackBuilder& builder,
                                                                bool verbose) {

  if(_containsCondition) {
    _modCondition->removeMemberUnchecked(_modCondition->numMembers() - 1);
    _containsCondition = false;
  }
  _modCondition->toVelocyPack(builder, verbose);
}

TraversalNode::TraversalNode(ExecutionPlan* plan, ExecutionNodeId id,
                             TRI_vocbase_t* vocbase, AstNode const* direction,
                             AstNode const* start, AstNode const* graph,
                             std::unique_ptr<Expression> pruneExpression,
                             std::unique_ptr<BaseOptions> options)
    : GraphNode(plan, id, vocbase, direction, graph, std::move(options)),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr),
      _pruneExpression(std::move(pruneExpression)) {
  TRI_ASSERT(start != nullptr);

  auto ast = _plan->getAst();
  // Let us build the conditions on _from and _to. Just in case we need them.
  {
    auto const* access =
        ast->createNodeAttributeAccess(_tmpObjVarNode, StaticStrings::FromString.c_str(),
                                       StaticStrings::FromString.length());
    _fromCondition = ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ,
                                                   access, _tmpIdNode);
  }
  TRI_ASSERT(_fromCondition != nullptr);
  TRI_ASSERT(_fromCondition->type == NODE_TYPE_OPERATOR_BINARY_EQ);

  {
    auto const* access =
        ast->createNodeAttributeAccess(_tmpObjVarNode, StaticStrings::ToString.c_str(),
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

  if (_pruneExpression) {
    _pruneExpression->variables(_pruneVariables);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
}

/// @brief Internal constructor to clone the node.
TraversalNode::TraversalNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                             std::vector<Collection*> const& edgeColls,
                             std::vector<Collection*> const& vertexColls,
                             Variable const* inVariable, std::string const& vertexId,
                             TRI_edge_direction_e defaultDirection,
                             std::vector<TRI_edge_direction_e> const& directions,
                             std::unique_ptr<graph::BaseOptions> options,
                             graph::Graph const* graph)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, defaultDirection,
                directions, std::move(options), graph),
      _pathOutVariable(nullptr),
      _inVariable(inVariable),
      _vertexId(vertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {}

TraversalNode::TraversalNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _pathOutVariable(nullptr),
      _inVariable(nullptr),
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
    for (VPackSlice v : VPackArrayIterator(list)) {
      _conditionVariables.emplace(_plan->getAst()->variables()->createVariable(v));
    }
  }

  // Out variables
  if (base.hasKey("pathOutVariable")) {
    _pathOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "pathOutVariable");
  }

  // Filter Condition Parts
  TRI_ASSERT(base.hasKey("fromCondition"));
  _fromCondition = plan->getAst()->createNode(base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  _toCondition = plan->getAst()->createNode(base.get("toCondition"));

  list = base.get("globalEdgeConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalEdgeConditions.emplace_back(plan->getAst()->createNode(cond));
    }
  }

  list = base.get("globalVertexConditions");
  if (list.isArray()) {
    for (auto const& cond : VPackArrayIterator(list)) {
      _globalVertexConditions.emplace_back(plan->getAst()->createNode(cond));
    }
  }

  list = base.get("vertexConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      _vertexConditions.try_emplace(StringUtils::uint64(key),
                                    plan->getAst()->createNode(cond.value));
    }
  }

  list = base.get("edgeConditions");
  if (list.isObject()) {
    for (auto const& cond : VPackObjectIterator(list)) {
      std::string key = cond.key.copyString();
      auto ecbuilder = std::make_unique<TraversalEdgeConditionBuilder>(this, cond.value);
      _edgeConditions.try_emplace(StringUtils::uint64(key), std::move(ecbuilder));
    }
  }

  list = base.get("expression");
  if (!list.isNone()) {
    _pruneExpression = std::make_unique<aql::Expression>(plan->getAst(), base);
    TRI_ASSERT(base.hasKey("pruneVariables"));
    list = base.get("pruneVariables");
    TRI_ASSERT(list.isArray());
    for (auto const& varinfo : VPackArrayIterator(list)) {
      _pruneVariables.emplace(plan->getAst()->variables()->createVariable(varinfo));
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif
}

// This constructor is only used from LocalTraversalNode, and GraphNode
// is virtually inherited; thus its constructor is never called from here.
TraversalNode::TraversalNode(ExecutionPlan& plan, TraversalNode const& other,
                             bool const allowAlreadyBuiltCopy)
    : GraphNode(GraphNode::THIS_THROWS_WHEN_CALLED{}),
      _pathOutVariable(nullptr),
      _inVariable(other._inVariable),
      _vertexId(other._vertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  if (!allowAlreadyBuiltCopy) {
    TRI_ASSERT(!other._optionsBuilt);
  }
  other.traversalCloneHelper(plan, *this, false);
}

TraversalNode::~TraversalNode() = default;


/// @brief return the path out variable
Variable const* TraversalNode::pathOutVariable() const {
  return _pathOutVariable;
}

/// @brief set the path out variable
void TraversalNode::setPathOutput(Variable const* outVar) {
  _pathOutVariable = outVar;
}

/// @brief return the in variable
Variable const* TraversalNode::inVariable() const { return _inVariable; }

std::string const TraversalNode::getStartVertex() const { return _vertexId; }

void TraversalNode::setInVariable(Variable const* inVariable) {
  TRI_ASSERT(_inVariable == nullptr);
  _inVariable = inVariable;
  _vertexId = "";
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

void TraversalNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                       std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  GraphNode::toVelocyPackHelper(nodes, flags, seen);
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
    _condition->toVelocyPack(nodes, flags != 0);
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
  _fromCondition->toVelocyPack(nodes, flags);

  TRI_ASSERT(_toCondition != nullptr);
  nodes.add(VPackValue("toCondition"));
  _toCondition->toVelocyPack(nodes, flags);

  if (!_globalEdgeConditions.empty()) {
    nodes.add(VPackValue("globalEdgeConditions"));
    nodes.openArray();
    for (auto const& it : _globalEdgeConditions) {
      it->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_globalVertexConditions.empty()) {
    nodes.add(VPackValue("globalVertexConditions"));
    nodes.openArray();
    for (auto const& it : _globalVertexConditions) {
      it->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_vertexConditions.empty()) {
    nodes.add(VPackValue("vertexConditions"));
    nodes.openObject();
    for (auto const& it : _vertexConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (!_edgeConditions.empty()) {
    nodes.add(VPackValue("edgeConditions"));
    nodes.openObject();
    for (auto& it : _edgeConditions) {
      nodes.add(VPackValue(basics::StringUtils::itoa(it.first)));
      it.second->toVelocyPack(nodes, flags);
    }
    nodes.close();
  }

  if (_pruneExpression != nullptr) {
    // The Expression constructor expects only this name
    nodes.add(VPackValue("expression"));
    _pruneExpression->toVelocyPack(nodes, flags);
    nodes.add(VPackValue("pruneVariables"));
    nodes.openArray();
    for (auto const& var : _pruneVariables) {
      var->toVelocyPack(nodes);
    }
    nodes.close();
  }
  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> TraversalNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  auto inputRegisters = RegIdSet{};
  auto& varInfo = getRegisterPlan()->varInfo;
  RegisterId inputRegister = RegisterPlan::MaxRegisterId;
  if (usesInVariable()) {
    auto it = varInfo.find(inVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    inputRegisters.emplace(it->second.registerId);
    inputRegister = it->second.registerId;
    TRI_ASSERT(getStartVertex().empty());
  }
  auto outputRegisters = RegIdSet{};
  std::unordered_map<TraversalExecutorInfos::OutputName, RegisterId, TraversalExecutorInfos::OutputNameHash> outputRegisterMapping;

  if (usesVertexOutVariable()) {
    auto it = varInfo.find(vertexOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < RegisterPlan::MaxRegisterId);
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(TraversalExecutorInfos::OutputName::VERTEX,
                                      it->second.registerId);
  }
  if (usesEdgeOutVariable()) {
    auto it = varInfo.find(edgeOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < RegisterPlan::MaxRegisterId);
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(TraversalExecutorInfos::OutputName::EDGE,
                                      it->second.registerId);
  }
  if (usesPathOutVariable()) {
    auto it = varInfo.find(pathOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    TRI_ASSERT(it->second.registerId < RegisterPlan::MaxRegisterId);
    outputRegisters.emplace(it->second.registerId);
    outputRegisterMapping.try_emplace(TraversalExecutorInfos::OutputName::PATH,
                                      it->second.registerId);
  }
  TraverserOptions* opts = this->options();
  std::unique_ptr<Traverser> traverser;

  if (pruneExpression() != nullptr) {
    std::vector<Variable const*> pruneVars;
    getPruneVariables(pruneVars);
    std::vector<RegisterId> pruneRegs;
    // Create List for _pruneVars
    pruneRegs.reserve(pruneVars.size());
    size_t vertexRegIdx = std::numeric_limits<std::size_t>::max();
    size_t edgeRegIdx = std::numeric_limits<std::size_t>::max();
    size_t pathRegIdx = std::numeric_limits<std::size_t>::max();
    for (auto const v : pruneVars) {
      if (v == vertexOutVariable()) {
        vertexRegIdx = pruneRegs.size();
        pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
      } else if (v == edgeOutVariable()) {
        edgeRegIdx = pruneRegs.size();
        pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
      } else if (v == pathOutVariable()) {
        pathRegIdx = pruneRegs.size();
        pruneRegs.emplace_back(RegisterPlan::MaxRegisterId);
      } else {
        auto it = varInfo.find(v->id);
        TRI_ASSERT(it != varInfo.end());
        pruneRegs.emplace_back(it->second.registerId);
      }
    }

    opts->activatePrune(std::move(pruneVars), std::move(pruneRegs),
                        vertexRegIdx, edgeRegIdx, pathRegIdx, pruneExpression());
  }

  if (arangodb::ServerState::instance()->isCoordinator()) {
#ifdef USE_ENTERPRISE
    waitForSatelliteIfRequired(&engine);
    if (isSmart() && !isDisjoint()) {
      traverser =
          std::make_unique<arangodb::traverser::SmartGraphTraverser>(opts, engines());
    } else {
#endif
      traverser = std::make_unique<arangodb::traverser::ClusterTraverser>(
          opts, engines(), engine.getQuery().vocbase().name());
#ifdef USE_ENTERPRISE
    }
#endif
  } else {
    traverser = std::make_unique<arangodb::traverser::SingleServerTraverser>(opts);
  }

  // Optimized condition
  std::vector<std::pair<Variable const*, RegisterId>> filterConditionVariables;
  filterConditionVariables.reserve(_conditionVariables.size());
  for (auto const& it : _conditionVariables) {
    if (it != _tmpObjVariable) {
      auto idIt = varInfo.find(it->id);
      TRI_ASSERT(idIt != varInfo.end());
      filterConditionVariables.emplace_back(std::make_pair(it, idIt->second.registerId));
      inputRegisters.emplace(idIt->second.registerId);
    }
  }

  auto registerInfos =
      createRegisterInfos(std::move(inputRegisters), std::move(outputRegisters));

  TRI_ASSERT(traverser != nullptr);
  auto executorInfos = TraversalExecutorInfos(std::move(traverser), outputRegisterMapping,
                                              getStartVertex(), inputRegister,
                                              std::move(filterConditionVariables));

  return std::make_unique<ExecutionBlockImpl<TraversalExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

/// @brief clone ExecutionNode recursively
ExecutionNode* TraversalNode::clone(ExecutionPlan* plan, bool withDependencies,
                                    bool withProperties) const {
  auto* oldOpts = options();
  std::unique_ptr<BaseOptions> tmp = std::make_unique<TraverserOptions>(*oldOpts, /*allowAlreadyBuiltCopy*/true);
  auto c = std::make_unique<TraversalNode>(plan, _id, _vocbase, _edgeColls, _vertexColls,
                                           _inVariable, _vertexId, _defaultDirection,
                                           _directions, std::move(tmp), _graphObj);

  traversalCloneHelper(*plan, *c, withProperties);

  if (_optionsBuilt) {
    c->prepareOptions();
  }

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

void TraversalNode::traversalCloneHelper(ExecutionPlan& plan, TraversalNode& c,
                                         bool const withProperties) const {
  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable = plan.getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c.setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable = plan.getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c.setEdgeOutput(edgeOutVariable);
  }

  if (usesPathOutVariable()) {
    auto pathOutVariable = _pathOutVariable;
    if (withProperties) {
      pathOutVariable = plan.getAst()->variables()->createVariable(pathOutVariable);
    }
    TRI_ASSERT(pathOutVariable != nullptr);
    c.setPathOutput(pathOutVariable);
  }

  c._conditionVariables.reserve(_conditionVariables.size());
  for (auto const& it : _conditionVariables) {
//#warning TODO: check if not cloning variables breaks anything
    if (withProperties) {
      c._conditionVariables.emplace(it->clone());
    } else {
      c._conditionVariables.emplace(it);
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  checkConditionsDefined();
#endif

  // Temporary Filter Objects
  c._tmpObjVariable = _tmpObjVariable;
  c._tmpObjVarNode = _tmpObjVarNode;
  c._tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c._fromCondition = _fromCondition->clone(_plan->getAst());
  c._toCondition = _toCondition->clone(_plan->getAst());
  c._globalEdgeConditions.insert(c._globalEdgeConditions.end(),
                                 _globalEdgeConditions.begin(),
                                 _globalEdgeConditions.end());
  c._globalVertexConditions.insert(c._globalVertexConditions.end(),
                                   _globalVertexConditions.begin(),
                                   _globalVertexConditions.end());

  for (auto const& it : _edgeConditions) {
    // Copy the builder
    auto ecBuilder =
        std::make_unique<TraversalEdgeConditionBuilder>(this, it.second.get());
    c._edgeConditions.try_emplace(it.first, std::move(ecBuilder));
  }

  for (auto const& it : _vertexConditions) {
    c._vertexConditions.try_emplace(it.first, it.second->clone(_plan->getAst()));
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  c.checkConditionsDefined();
#endif
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
            _plan, _edgeColls[i]->name(), StaticStrings::ToString,
            globalEdgeConditionBuilder.getInboundCondition()->clone(ast));
        break;
      case TRI_EDGE_OUT:
        _options->addLookupInfo(
            _plan, _edgeColls[i]->name(), StaticStrings::FromString,
            globalEdgeConditionBuilder.getOutboundCondition()->clone(ast));
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
  }


  TraverserOptions* opts = this->TraversalNode::options();
  TRI_ASSERT(opts != nullptr);
  /*
   * HACK: DO NOT use other indexes for smart BFS. Otherwise this will produce
   * wrong results.
   */
  bool onlyEdgeIndexes = this->isSmart() && opts->isUseBreadthFirst();
  for (auto& it : _edgeConditions) {
    uint64_t depth = it.first;
    // We probably have to adopt minDepth. We cannot fulfill a condition of
    // larger depth anyway
    auto& builder = it.second;

    for (auto& it2 : _globalEdgeConditions) {
      builder->addConditionPart(it2);
    }

    for (size_t i = 0; i < numEdgeColls; ++i) {
      auto dir = _directions[i];
      // TODO we can optimize here. indexCondition and Expression could be
      // made non-overlapping.
      switch (dir) {
        case TRI_EDGE_IN:
          opts->addDepthLookupInfo(_plan, _edgeColls[i]->name(), StaticStrings::ToString,
                                   builder->getInboundCondition()->clone(ast), depth, onlyEdgeIndexes);
          break;
        case TRI_EDGE_OUT:
          opts->addDepthLookupInfo(_plan, _edgeColls[i]->name(), StaticStrings::FromString,
                                   builder->getOutboundCondition()->clone(ast), depth, onlyEdgeIndexes);
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
    opts->_vertexExpressions.try_emplace(
      it.first,
      arangodb::lazyConstruct([&]{
        return new Expression(ast, it.second);
      })
    );
  }
  if (!_globalVertexConditions.empty()) {
    auto cond = _plan->getAst()->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    for (auto const& it : _globalVertexConditions) {
      cond->addMember(it);
    }
    opts->_baseVertexExpression.reset(new Expression(ast, cond));
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
void TraversalNode::setCondition(std::unique_ptr<arangodb::aql::Condition> condition) {
  VarSet varsUsedByCondition;

  Ast::getReferencedVariables(condition->root(), varsUsedByCondition);

  for (auto const& oneVar : varsUsedByCondition) {
    if ((_vertexOutVariable == nullptr || oneVar->id != _vertexOutVariable->id) &&
        (_edgeOutVariable == nullptr || oneVar->id != _edgeOutVariable->id) &&
        (_pathOutVariable == nullptr || oneVar->id != _pathOutVariable->id) &&
        (_inVariable == nullptr || oneVar->id != _inVariable->id)) {
      _conditionVariables.emplace(oneVar);
    }
  }

  _condition = std::move(condition);
}

void TraversalNode::registerCondition(bool isConditionOnEdge, uint64_t conditionLevel,
                                      AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    auto [it, emplaced] = _edgeConditions.try_emplace(
        conditionLevel, arangodb::lazyConstruct([&]() -> std::unique_ptr<TraversalEdgeConditionBuilder> {
          auto builder = std::make_unique<TraversalEdgeConditionBuilder>(this);
          builder->addConditionPart(condition);
          return builder;
        }));
    if (!emplaced) {
      it->second->addConditionPart(condition);
    }
  } else {
    auto [it, emplaced] =
        _vertexConditions.try_emplace(conditionLevel, arangodb::lazyConstruct([&] {
                                        auto cond = _plan->getAst()->createNodeNaryOperator(
                                            NODE_TYPE_OPERATOR_NARY_AND);
                                        cond->addMember(condition);
                                        return cond;
                                      }));
    if (!emplaced) {
      it->second->addMember(condition);
    }
  }
}

void TraversalNode::registerGlobalCondition(bool isConditionOnEdge, AstNode const* condition) {
  Ast::getReferencedVariables(condition, _conditionVariables);
  if (isConditionOnEdge) {
    _globalEdgeConditions.emplace_back(condition);
  } else {
    _globalVertexConditions.emplace_back(condition);
  }
}

void TraversalNode::getConditionVariables(std::vector<Variable const*>& res) const {
  for (auto const& it : _conditionVariables) {
    if (it != _tmpObjVariable) {
      res.emplace_back(it);
    }
  }
}

void TraversalNode::getPruneVariables(std::vector<Variable const*>& res) const {
  if (_pruneExpression) {
    for (auto const& it : _pruneVariables) {
      res.emplace_back(it);
    }
  }
}

bool TraversalNode::usesPathOutVariable() const {
  return _pathOutVariable != nullptr && options()->producePaths();
}

auto TraversalNode::options() const -> TraverserOptions* {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* opts = dynamic_cast<TraverserOptions*>(GraphNode::options());
  TRI_ASSERT((GraphNode::options() == nullptr) == (opts == nullptr));
#else
  auto* opts = static_cast<TraverserOptions*>(GraphNode::options());
#endif
  return opts;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
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
