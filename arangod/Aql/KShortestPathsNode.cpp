/// @brief Implementation of k Shortest Path Execution Node
///
/// @file arangod/Aql/KShortestPathsNode.cpp
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
/// @author Markus Pfeiffer
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "KShortestPathsNode.h"

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/KShortestPathsExecutor.h"
#include "Aql/Query.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Graph/AttributeWeightShortestPathFinder.h"
#include "Graph/KShortestPathsFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <memory>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static void parseNodeInput(AstNode const* node, std::string& id,
                           Variable const*& variable, char const* part) {
  switch (node->type) {
    case NODE_TYPE_REFERENCE:
      variable = static_cast<Variable*>(node->getData());
      id = "";
      break;
    case NODE_TYPE_VALUE:
      if (node->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_QUERY_PARSE, std::string("invalid ") + part +
                                       " vertex. Must either be "
                                       "an _id string or an object with _id.");
      }
      variable = nullptr;
      id = node->getString();
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     std::string("invalid ") + part +
                                         " vertex. Must either be an "
                                         "_id string or an object with _id.");
  }
}
static KShortestPathsExecutorInfos::InputVertex prepareVertexInput(KShortestPathsNode const* node,
                                                                   bool isTarget) {
  using InputVertex = KShortestPathsExecutorInfos::InputVertex;
  if (isTarget) {
    if (node->usesTargetInVariable()) {
      auto it = node->getRegisterPlan()->varInfo.find(node->targetInVariable().id);
      TRI_ASSERT(it != node->getRegisterPlan()->varInfo.end());
      return InputVertex{it->second.registerId};
    } else {
      return InputVertex{node->getTargetVertex()};
    }
  } else {
    if (node->usesStartInVariable()) {
      auto it = node->getRegisterPlan()->varInfo.find(node->startInVariable().id);
      TRI_ASSERT(it != node->getRegisterPlan()->varInfo.end());
      return InputVertex{it->second.registerId};
    } else {
      return InputVertex{node->getStartVertex()};
    }
  }
}
}  // namespace

KShortestPathsNode::KShortestPathsNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                                       AstNode const* direction, AstNode const* start,
                                       AstNode const* target, AstNode const* graph,
                                       std::unique_ptr<BaseOptions> options)
    : GraphNode(plan, id, vocbase, direction, graph, std::move(options)),
      _pathOutVariable(nullptr),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  TRI_ASSERT(start != nullptr);
  TRI_ASSERT(target != nullptr);
  TRI_ASSERT(graph != nullptr);

  auto ast = _plan->getAst();
  // Let us build the conditions on _from and _to. Just in case we need them.
  {
    auto const* access =
        ast->createNodeAttributeAccess(getTemporaryRefNode(),
                                       StaticStrings::FromString.c_str(),
                                       StaticStrings::FromString.length());
    auto const* cond =
        ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _fromCondition->addMember(cond);
  }
  TRI_ASSERT(_fromCondition != nullptr);

  {
    auto const* access =
        ast->createNodeAttributeAccess(getTemporaryRefNode(),
                                       StaticStrings::ToString.c_str(),
                                       StaticStrings::ToString.length());
    auto const* cond =
        ast->createNodeBinaryOperator(NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _toCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _toCondition->addMember(cond);
  }
  TRI_ASSERT(_toCondition != nullptr);

  parseNodeInput(start, _startVertexId, _inStartVariable, "start");
  parseNodeInput(target, _targetVertexId, _inTargetVariable, "target");
}

/// @brief Internal constructor to clone the node.
KShortestPathsNode::KShortestPathsNode(
    ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
    std::vector<Collection*> const& edgeColls,
    std::vector<Collection*> const& vertexColls,
    TRI_edge_direction_e defaultDirection, std::vector<TRI_edge_direction_e> const& directions,
    Variable const* inStartVariable, std::string const& startVertexId,
    Variable const* inTargetVariable, std::string const& targetVertexId,
    std::unique_ptr<graph::BaseOptions> options, graph::Graph const* graph)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, defaultDirection,
                directions, std::move(options), graph),
      _pathOutVariable(nullptr),
      _inStartVariable(inStartVariable),
      _startVertexId(startVertexId),
      _inTargetVariable(inTargetVariable),
      _targetVertexId(targetVertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {}

KShortestPathsNode::~KShortestPathsNode() = default;

KShortestPathsNode::KShortestPathsNode(ExecutionPlan* plan,
                                       arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _pathOutVariable(nullptr),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  // Path out variable
  if (base.hasKey("pathOutVariable")) {
    _pathOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "pathOutVariable");
  }

  // Start Vertex
  if (base.hasKey("startInVariable")) {
    _inStartVariable =
        Variable::varFromVPack(plan->getAst(), base, "startInVariable");
  } else {
    VPackSlice v = base.get("startVertexId");
    if (!v.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex must be a string");
    }
    _startVertexId = v.copyString();

    if (_startVertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "start vertex mustn't be empty");
    }
  }

  // Target Vertex
  if (base.hasKey("targetInVariable")) {
    _inTargetVariable =
        Variable::varFromVPack(plan->getAst(), base, "targetInVariable");
  } else {
    VPackSlice v = base.get("targetVertexId");
    if (!v.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "target vertex must be a string");
    }
    _targetVertexId = v.copyString();
    if (_targetVertexId.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "target vertex mustn't be empty");
    }
  }

  // Filter Condition Parts
  TRI_ASSERT(base.hasKey("fromCondition"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _fromCondition = new AstNode(plan->getAst(), base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _toCondition = new AstNode(plan->getAst(), base.get("toCondition"));
}

// This constructor is only used from LocalTraversalNode, and GraphNode
// is virtually inherited; thus its constructor is never called from here.
KShortestPathsNode::KShortestPathsNode(ExecutionPlan& plan, KShortestPathsNode const& other)
    : GraphNode(GraphNode::THIS_THROWS_WHEN_CALLED{}),
      _pathOutVariable(other._pathOutVariable),
      _inStartVariable(other._inStartVariable),
      _startVertexId(other._startVertexId),
      _inTargetVariable(other._inTargetVariable),
      _targetVertexId(other._targetVertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  other.kShortestPathsCloneHelper(plan, *this, false);
}

void KShortestPathsNode::setStartInVariable(Variable const* inVariable) {
  TRI_ASSERT(_inStartVariable == nullptr);
  _inStartVariable = inVariable;
  _startVertexId = "";
}

void KShortestPathsNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                            std::unordered_set<ExecutionNode const*>& seen) const {
  GraphNode::toVelocyPackHelper(nodes, flags, seen);  // call base class method
  // Out variables
  if (usesPathOutVariable()) {
    nodes.add(VPackValue("pathOutVariable"));
    pathOutVariable().toVelocyPack(nodes);
  }

  // In variables
  if (usesStartInVariable()) {
    nodes.add(VPackValue("startInVariable"));
    startInVariable().toVelocyPack(nodes);
  } else {
    nodes.add("startVertexId", VPackValue(_startVertexId));
  }

  if (usesTargetInVariable()) {
    nodes.add(VPackValue("targetInVariable"));
    targetInVariable().toVelocyPack(nodes);
  } else {
    nodes.add("targetVertexId", VPackValue(_targetVertexId));
  }

  // Filter Conditions
  TRI_ASSERT(_fromCondition != nullptr);
  nodes.add(VPackValue("fromCondition"));
  _fromCondition->toVelocyPack(nodes, flags);

  TRI_ASSERT(_toCondition != nullptr);
  nodes.add(VPackValue("toCondition"));
  _toCondition->toVelocyPack(nodes, flags);

  // And close it:
  nodes.close();
}

/// @brief creates corresponding ExecutionBlock
std::unique_ptr<ExecutionBlock> KShortestPathsNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  RegIdSet inputRegisters;
  if (usesStartInVariable()) {
    inputRegisters.emplace(variableToRegisterId(&startInVariable()));
  }
  if (usesTargetInVariable()) {
    inputRegisters.emplace(variableToRegisterId(&targetInVariable()));
  }

  TRI_ASSERT(usesPathOutVariable());  // This node always produces the path!
  auto outputRegister = variableToRegisterId(&pathOutVariable());
  auto outputRegisters = RegIdSet{outputRegister};

  auto registerInfos = createRegisterInfos(std::move(inputRegisters), std::move(outputRegisters));

  auto opts = static_cast<ShortestPathOptions*>(options());

  KShortestPathsExecutorInfos::InputVertex sourceInput = ::prepareVertexInput(this, false);
  KShortestPathsExecutorInfos::InputVertex targetInput = ::prepareVertexInput(this, true);

#ifdef USE_ENTERPRISE
  waitForSatelliteIfRequired(&engine);
#endif

  std::unique_ptr<KShortestPathsFinder> finder;
  finder = std::make_unique<graph::KShortestPathsFinder>(*opts);

  TRI_ASSERT(finder != nullptr);
  auto executorInfos =
      KShortestPathsExecutorInfos(outputRegister, std::move(finder),
                                  std::move(sourceInput), std::move(targetInput));
  return std::make_unique<ExecutionBlockImpl<KShortestPathsExecutor>>(
      &engine, this, std::move(registerInfos), std::move(executorInfos));
}

ExecutionNode* KShortestPathsNode::clone(ExecutionPlan* plan, bool withDependencies,
                                         bool withProperties) const {
  TRI_ASSERT(!_optionsBuilt);
  auto oldOpts = static_cast<ShortestPathOptions*>(options());
  std::unique_ptr<BaseOptions> tmp = std::make_unique<ShortestPathOptions>(*oldOpts);
  auto c = std::make_unique<KShortestPathsNode>(plan, _id, _vocbase, _edgeColls,
                                                _vertexColls, _defaultDirection, _directions,
                                                _inStartVariable, _startVertexId,
                                                _inTargetVariable, _targetVertexId,
                                                std::move(tmp), _graphObj);

  kShortestPathsCloneHelper(*plan, *c, withProperties);

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

void KShortestPathsNode::kShortestPathsCloneHelper(ExecutionPlan& plan,
                                                   KShortestPathsNode& c,
                                                   bool withProperties) const {
  if (usesPathOutVariable()) {
    auto pathOutVariable = _pathOutVariable;
    if (withProperties) {
      pathOutVariable = plan.getAst()->variables()->createVariable(pathOutVariable);
    }
    TRI_ASSERT(pathOutVariable != nullptr);
    c.setPathOutput(pathOutVariable);
  }

  // Temporary Filter Objects
  c._tmpObjVariable = _tmpObjVariable;
  c._tmpObjVarNode = _tmpObjVarNode;
  c._tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c._fromCondition = _fromCondition->clone(_plan->getAst());
  c._toCondition = _toCondition->clone(_plan->getAst());
}

void KShortestPathsNode::prepareOptions() {
  if (_optionsBuilt) {
    return;
  }
  TRI_ASSERT(!_optionsBuilt);

  size_t numEdgeColls = _edgeColls.size();
  Ast* ast = _plan->getAst();
  auto opts = static_cast<ShortestPathOptions*>(options());

  opts->setVariable(getTemporaryVariable());

  // Compute Indexes.
  for (size_t i = 0; i < numEdgeColls; ++i) {
    auto dir = _directions[i];
    switch (dir) {
      case TRI_EDGE_IN:
        opts->addLookupInfo(_plan, _edgeColls[i]->name(),
                            StaticStrings::ToString, _toCondition->clone(ast));
        opts->addReverseLookupInfo(_plan, _edgeColls[i]->name(), StaticStrings::FromString,
                                   _fromCondition->clone(ast));
        break;
      case TRI_EDGE_OUT:
        opts->addLookupInfo(_plan, _edgeColls[i]->name(),
                            StaticStrings::FromString, _fromCondition->clone(ast));
        opts->addReverseLookupInfo(_plan, _edgeColls[i]->name(), StaticStrings::ToString,
                                   _toCondition->clone(ast));
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
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

auto KShortestPathsNode::options() const -> graph::ShortestPathOptions* {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* opts = dynamic_cast<ShortestPathOptions*>(GraphNode::options());
  TRI_ASSERT((GraphNode::options() == nullptr) == (opts == nullptr));
#else
  auto* opts = static_cast<ShortestPathOptions*>(GraphNode::options());
#endif
  return opts;
}
