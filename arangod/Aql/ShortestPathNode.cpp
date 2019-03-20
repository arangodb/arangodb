/// @brief Implementation of Shortest Path Execution Node
///
/// @file arangod/Aql/ShortestPathNode.cpp
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

#include "ShortestPathNode.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Aql/ShortestPathExecutor.h"
#include "Graph/AttributeWeightShortestPathFinder.h"
#include "Graph/ConstantWeightShortestPathFinder.h"
#include "Graph/ShortestPathFinder.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/ShortestPathResult.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;
using namespace arangodb::graph;

namespace {
static void parseNodeInput(AstNode const* node, std::string& id, Variable const*& variable) {
  switch (node->type) {
    case NODE_TYPE_REFERENCE:
      variable = static_cast<Variable*>(node->getData());
      id = "";
      break;
    case NODE_TYPE_VALUE:
      if (node->value.type != VALUE_TYPE_STRING) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                       "invalid start vertex. Must either be "
                                       "an _id string or an object with _id.");
      }
      variable = nullptr;
      id = node->getString();
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_PARSE,
                                     "invalid start vertex. Must either be an "
                                     "_id string or an object with _id.");
  }
}
static ShortestPathExecutorInfos::InputVertex prepareVertexInput(ShortestPathNode const* node,
                                                                 bool isTarget) {
  using InputVertex = ShortestPathExecutorInfos::InputVertex;
  if (isTarget) {
    if (node->usesTargetInVariable()) {
      auto it = node->getRegisterPlan()->varInfo.find(node->targetInVariable()->id);
      TRI_ASSERT(it != node->getRegisterPlan()->varInfo.end());
      return InputVertex{it->second.registerId};
    } else {
      return InputVertex{node->getTargetVertex()};
    }
  } else {
    if (node->usesStartInVariable()) {
      auto it = node->getRegisterPlan()->varInfo.find(node->startInVariable()->id);
      TRI_ASSERT(it != node->getRegisterPlan()->varInfo.end());
      return InputVertex{it->second.registerId};
    } else {
      return InputVertex{node->getStartVertex()};
    }
  }
}
}  // namespace

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
                                   AstNode const* direction, AstNode const* start,
                                   AstNode const* target, AstNode const* graph,
                                   std::unique_ptr<BaseOptions> options)
    : GraphNode(plan, id, vocbase, direction, graph, std::move(options)),
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

  parseNodeInput(start, _startVertexId, _inStartVariable);
  parseNodeInput(target, _targetVertexId, _inTargetVariable);
}

/// @brief Internal constructor to clone the node.
ShortestPathNode::ShortestPathNode(
    ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
    std::vector<std::unique_ptr<Collection>> const& edgeColls,
    std::vector<std::unique_ptr<Collection>> const& vertexColls,
    std::vector<TRI_edge_direction_e> const& directions, Variable const* inStartVariable,
    std::string const& startVertexId, Variable const* inTargetVariable,
    std::string const& targetVertexId, std::unique_ptr<BaseOptions> options)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, directions, std::move(options)),
      _inStartVariable(inStartVariable),
      _startVertexId(startVertexId),
      _inTargetVariable(inTargetVariable),
      _targetVertexId(targetVertexId),
      _fromCondition(nullptr),
      _toCondition(nullptr) {}

ShortestPathNode::~ShortestPathNode() {}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
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
  _fromCondition = new AstNode(plan->getAst(), base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  _toCondition = new AstNode(plan->getAst(), base.get("toCondition"));
}

void ShortestPathNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags) const {
  GraphNode::toVelocyPackHelper(nodes, flags);  // call base class method
  // In variables
  if (usesStartInVariable()) {
    nodes.add(VPackValue("startInVariable"));
    startInVariable()->toVelocyPack(nodes);
  } else {
    nodes.add("startVertexId", VPackValue(_startVertexId));
  }

  if (usesTargetInVariable()) {
    nodes.add(VPackValue("targetInVariable"));
    targetInVariable()->toVelocyPack(nodes);
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
std::unique_ptr<ExecutionBlock> ShortestPathNode::createBlock(
    ExecutionEngine& engine, std::unordered_map<ExecutionNode*, ExecutionBlock*> const&) const {
  ExecutionNode const* previousNode = getFirstDependency();
  TRI_ASSERT(previousNode != nullptr);
  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  auto& varInfo = getRegisterPlan()->varInfo;
  if (usesStartInVariable()) {
    auto it = varInfo.find(startInVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    inputRegisters->emplace(it->second.registerId);
  }
  if (usesTargetInVariable()) {
    auto it = varInfo.find(targetInVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    inputRegisters->emplace(it->second.registerId);
  }

  auto outputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  std::unordered_map<ShortestPathExecutorInfos::OutputName, RegisterId, ShortestPathExecutorInfos::OutputNameHash> outputRegisterMapping;
  if (usesVertexOutVariable()) {
    auto it = varInfo.find(vertexOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    outputRegisterMapping.emplace(ShortestPathExecutorInfos::OutputName::VERTEX,
                                  it->second.registerId);
    outputRegisters->emplace(it->second.registerId);
  }
  if (usesEdgeOutVariable()) {
    auto it = varInfo.find(edgeOutVariable()->id);
    TRI_ASSERT(it != varInfo.end());
    outputRegisterMapping.emplace(ShortestPathExecutorInfos::OutputName::EDGE,
                                  it->second.registerId);
    outputRegisters->emplace(it->second.registerId);
  }

  auto opts = static_cast<ShortestPathOptions*>(options());

  ShortestPathExecutorInfos::InputVertex sourceInput = ::prepareVertexInput(this, false);
  ShortestPathExecutorInfos::InputVertex targetInput = ::prepareVertexInput(this, true);

  std::unique_ptr<ShortestPathFinder> finder;
  if (opts->useWeight()) {
    finder.reset(new graph::AttributeWeightShortestPathFinder(*opts));
  } else {
    finder.reset(new graph::ConstantWeightShortestPathFinder(*opts));
  }

  TRI_ASSERT(finder != nullptr);
  ShortestPathExecutorInfos infos(inputRegisters, outputRegisters,
                                  getRegisterPlan()->nrRegs[previousNode->getDepth()],
                                  getRegisterPlan()->nrRegs[getDepth()],
                                  getRegsToClear(), calcRegsToKeep(),
                                  std::move(finder), std::move(outputRegisterMapping),
                                  std::move(sourceInput), std::move(targetInput));
  return std::make_unique<ExecutionBlockImpl<ShortestPathExecutor>>(
      &engine, this, std::move(infos));
}

ExecutionNode* ShortestPathNode::clone(ExecutionPlan* plan, bool withDependencies,
                                       bool withProperties) const {
  TRI_ASSERT(!_optionsBuilt);
  auto oldOpts = static_cast<ShortestPathOptions*>(options());
  std::unique_ptr<BaseOptions> tmp = std::make_unique<ShortestPathOptions>(*oldOpts);
  auto c = std::make_unique<ShortestPathNode>(plan, _id, _vocbase, _edgeColls,
                                              _vertexColls, _directions, _inStartVariable,
                                              _startVertexId, _inTargetVariable,
                                              _targetVertexId, std::move(tmp));
  if (usesVertexOutVariable()) {
    auto vertexOutVariable = _vertexOutVariable;
    if (withProperties) {
      vertexOutVariable = plan->getAst()->variables()->createVariable(vertexOutVariable);
    }
    TRI_ASSERT(vertexOutVariable != nullptr);
    c->setVertexOutput(vertexOutVariable);
  }

  if (usesEdgeOutVariable()) {
    auto edgeOutVariable = _edgeOutVariable;
    if (withProperties) {
      edgeOutVariable = plan->getAst()->variables()->createVariable(edgeOutVariable);
    }
    TRI_ASSERT(edgeOutVariable != nullptr);
    c->setEdgeOutput(edgeOutVariable);
  }

  // Temporary Filter Objects
  c->_tmpObjVariable = _tmpObjVariable;
  c->_tmpObjVarNode = _tmpObjVarNode;
  c->_tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c->_fromCondition = _fromCondition->clone(_plan->getAst());
  c->_toCondition = _toCondition->clone(_plan->getAst());

  return cloneHelper(std::move(c), withDependencies, withProperties);
}

void ShortestPathNode::prepareOptions() {
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
