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
#include "Aql/ExecutionPlan.h"
#include "Aql/Query.h"
#include "Cluster/ClusterComm.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;

static void parseNodeInput(AstNode const* node, std::string& id,
                           Variable const*& variable) {
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

ShortestPathNode::ShortestPathNode(
    ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase, AstNode const* direction,
    AstNode const* start, AstNode const* target, AstNode const* graph,
    traverser::ShortestPathOptions* options)
    : GraphNode(plan, id, vocbase, options),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr) {
  TRI_ASSERT(start != nullptr);
  TRI_ASSERT(target != nullptr);

  // We have to call this here because we need a specific implementation
  // for shortest_path_node.
  parseGraphAstNodes(direction, graph);

  auto ast = _plan->getAst();
  // Let us build the conditions on _from and _to. Just in case we need them.
  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::FromString.c_str(),
        StaticStrings::FromString.length());
    auto const* condition = ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _fromCondition->addMember(condition);

  }
  TRI_ASSERT(_fromCondition != nullptr);

  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::ToString.c_str(),
        StaticStrings::ToString.length());
    auto const* condition = ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _toCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _toCondition->addMember(condition);
  }
  TRI_ASSERT(_toCondition != nullptr);
  parseNodeInput(start, _startVertexId, _inStartVariable);
  parseNodeInput(target, _targetVertexId, _inTargetVariable);
}

ShortestPathNode::ShortestPathNode(
    ExecutionPlan* plan, size_t id, TRI_vocbase_t* vocbase,
    std::vector<std::unique_ptr<aql::Collection>> const& edgeColls,
    std::vector<std::unique_ptr<aql::Collection>> const& reverseEdgeColls,
    std::vector<std::unique_ptr<aql::Collection>> const& vertexColls,
    std::vector<TRI_edge_direction_e> const& directions,
    Variable const* inStartVariable, std::string const& startVertexId,
    Variable const* inTargetVariable, std::string const& targetVertexId,
    traverser::ShortestPathOptions* options)
    : GraphNode(plan, id, vocbase, edgeColls, vertexColls, directions, options),
      _inStartVariable(inStartVariable),
      _startVertexId(startVertexId),
      _inTargetVariable(inTargetVariable),
      _targetVertexId(targetVertexId) {
  for (auto const& it : reverseEdgeColls) {
    // Collections cannot be copied. So we need to create new ones to prevent leaks
    _reverseEdgeColls.emplace_back(std::make_unique<aql::Collection>(
        it->getName(), _vocbase, AccessMode::Type::READ));
  }
}

ShortestPathNode::~ShortestPathNode() {
}

arangodb::traverser::ShortestPathOptions* ShortestPathNode::options()
    const {
  return static_cast<traverser::ShortestPathOptions*>(_options.get());
}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr) {

  // Reverse Collections
  VPackSlice list = base.get("reverseEdgeCollections");
  if (!list.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "shortest path needs an array of reverse edge collections.");
  }

  for (auto const& it : VPackArrayIterator(list)) {
    std::string e = arangodb::basics::VelocyPackHelper::getStringValue(it, "");
    _reverseEdgeColls.emplace_back(
        std::make_unique<aql::Collection>(e, _vocbase, AccessMode::Type::READ));
  }

  _options = std::make_unique<arangodb::traverser::ShortestPathOptions>(
      plan->getAst()->query()->trx());
  // Start Vertex
  if (base.hasKey("startInVariable")) {
    _inStartVariable = varFromVPack(plan->getAst(), base, "startInVariable");
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
    _inTargetVariable = varFromVPack(plan->getAst(), base, "targetInVariable");
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

  // Flags
  if (base.hasKey("shortestPathFlags")) {
    // _options = ShortestPathOptions(base);
  }
}

void ShortestPathNode::toVelocyPackHelper(VPackBuilder& nodes,
                                          bool verbose) const {
  baseToVelocyPackHelper(nodes, verbose);

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

  // Reverse collections
  nodes.add(VPackValue("reverseEdgeCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& e : _edgeColls) {
      nodes.add(VPackValue(e->getName()));
    }
  }


  // nodes.add(VPackValue("shortestPathFlags"));
  // _options.toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

ExecutionNode* ShortestPathNode::clone(ExecutionPlan* plan,
                                       bool withDependencies,
                                       bool withProperties) const {
  auto tmp =
      std::make_unique<arangodb::traverser::ShortestPathOptions>(*options());
  auto c = new ShortestPathNode(plan, _id, _vocbase, _edgeColls,
                                _reverseEdgeColls, _vertexColls, _directions,
                                _inStartVariable, _startVertexId,
                                _inTargetVariable, _targetVertexId, tmp.get());
  tmp.release();

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

  // Temporary Filter Objects
  c->_tmpObjVariable = _tmpObjVariable;
  c->_tmpObjVarNode = _tmpObjVarNode;
  c->_tmpIdNode = _tmpIdNode;

  // Filter Condition Parts
  c->_fromCondition = _fromCondition->clone(_plan->getAst());
  c->_toCondition = _toCondition->clone(_plan->getAst());
 
  cloneHelper(c, plan, withDependencies, withProperties);

  return static_cast<ExecutionNode*>(c);
}

void ShortestPathNode::prepareOptions() {
  if (_optionsBuild) {
    return;
  }

  size_t numEdgeColls = _edgeColls.size();
  TRI_ASSERT(!_optionsBuild);
  _options->setVariable(_tmpObjVariable);

  auto opts = dynamic_cast<arangodb::traverser::ShortestPathOptions*>(_options.get());

  Ast* ast = _plan->getAst();

  TRI_ASSERT(numEdgeColls == _directions.size());
  TRI_ASSERT(numEdgeColls == _reverseEdgeColls.size());
  // FIXME: _options->_baseLookupInfos.reserve(numEdgeColls);
  // Compute Edge Indexes. First default indexes:
  for (size_t i = 0; i < numEdgeColls; ++i) {
    switch (_directions[i]) {
      case TRI_EDGE_IN:
        opts->addLookupInfo(ast, _edgeColls[i]->getName(),
                            StaticStrings::ToString, _toCondition);
        opts->addReverseLookupInfo(ast, _reverseEdgeColls[i]->getName(),
                                   StaticStrings::FromString, _fromCondition);
        break;
      case TRI_EDGE_OUT:
        opts->addLookupInfo(ast, _edgeColls[i]->getName(),
                            StaticStrings::FromString, _fromCondition);
        opts->addReverseLookupInfo(ast, _reverseEdgeColls[i]->getName(),
                                   StaticStrings::ToString, _toCondition);
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
  }

  _optionsBuild = true;
}
