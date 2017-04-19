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
#include "Graph/ShortestPathOptions.h"
#include "Indexes/Index.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::aql;
using namespace arangodb::graph;

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

static TRI_edge_direction_e parseDirection (uint64_t dirNum) {
  switch (dirNum) {
    case 0:
      return TRI_EDGE_ANY;
    case 1:
      return TRI_EDGE_IN;
    case 2:
      return TRI_EDGE_OUT;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_PARSE,
          "direction can only be INBOUND, OUTBOUND or ANY");
  }
}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, size_t id,
                                   TRI_vocbase_t* vocbase, uint64_t direction,
                                   AstNode const* start, AstNode const* target,
                                   AstNode const* graph,
                                   std::unique_ptr<BaseOptions>& options)
    : GraphNode(plan, id, plan->getAst()->query()->vocbase(), options),
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
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::FromString.c_str(),
        StaticStrings::FromString.length());
    auto const* cond = ast->createNodeBinaryOperator(
        NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _fromCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _fromCondition->addMember(cond);
  }
  TRI_ASSERT(_fromCondition != nullptr);

  {
    auto const* access = ast->createNodeAttributeAccess(
        _tmpObjVarNode, StaticStrings::ToString.c_str(),
        StaticStrings::ToString.length());
    auto const* cond = ast->createNodeBinaryOperator(
      NODE_TYPE_OPERATOR_BINARY_EQ, access, _tmpIdNode);
    _toCondition = ast->createNodeNaryOperator(NODE_TYPE_OPERATOR_NARY_AND);
    _toCondition->addMember(cond);
  }
  TRI_ASSERT(_toCondition != nullptr);

  TRI_edge_direction_e baseDirection = parseDirection(direction);

  std::unordered_map<std::string, TRI_edge_direction_e> seenCollections;
  auto addEdgeColl = [&](std::string const& n, TRI_edge_direction_e dir) -> void {
    if (dir == TRI_EDGE_ANY) {
      _directions.emplace_back(TRI_EDGE_OUT);
      _edgeColls.emplace_back(n);

      _directions.emplace_back(TRI_EDGE_IN);
      _edgeColls.emplace_back(std::move(n));
    } else {
      _directions.emplace_back(dir);
      _edgeColls.emplace_back(std::move(n));
    }
  };

  auto ci = ClusterInfo::instance();

  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();
    auto resolver = std::make_unique<CollectionNameResolver>(vocbase);
    _graphInfo.openArray();
    _edgeColls.reserve(edgeCollectionCount);
    _directions.reserve(edgeCollectionCount);

    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      TRI_edge_direction_e dir = TRI_EDGE_ANY;
      auto col = graph->getMember(i);

      if (col->type == NODE_TYPE_DIRECTION) {
        TRI_ASSERT(col->numMembers() == 2);
        auto dirNode = col->getMember(0);
        // We have a collection with special direction.
        TRI_ASSERT(dirNode->isIntValue());
        dir = parseDirection(dirNode->getIntValue());
        col = col->getMember(1);
      } else {
        dir = baseDirection;
      }
 
      std::string eColName = col->getString();

      // now do some uniqueness checks for the specified collections
      auto it = seenCollections.find(eColName);
      if (it != seenCollections.end()) {
        if ((*it).second != dir) {
          std::string msg("conflicting directions specified for collection '" +
                          std::string(eColName));
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
                                         msg);
        }
        // do not re-add the same collection!
        continue;
      }
      seenCollections.emplace(eColName, dir);
 
      auto eColType = resolver->getCollectionTypeCluster(eColName);
      if (eColType != TRI_COL_TYPE_EDGE) {
        std::string msg("collection type invalid for collection '" +
                        std::string(eColName) +
                        ": expecting collection type 'edge'");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID,
                                       msg);
      }

      _graphInfo.add(VPackValue(eColName));
      if (ServerState::instance()->isRunningInCluster()) {
        auto c = ci->getCollection(_vocbase->name(), eColName);
        if (!c->isSmart()) {
          addEdgeColl(eColName, dir);
        } else {
          std::vector<std::string> names;
          names = c->realNamesForRead();
          for (auto const& name : names) {
            addEdgeColl(name, dir);
          }
        }
      } else {
        addEdgeColl(eColName, dir);
      }
    
      if (dir == TRI_EDGE_ANY) {
        // collection with direction ANY must be added again
        _graphInfo.add(VPackValue(eColName));
      }

    }
    _graphInfo.close();
  } else {
    if (_edgeColls.empty()) {
      if (graph->isStringValue()) {
        std::string graphName = graph->getString();
        _graphInfo.add(VPackValue(graphName));
        _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

        if (_graphObj == nullptr) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
        }

        auto eColls = _graphObj->edgeCollections();
        size_t length = eColls.size();
        if (length == 0) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
        }
        _edgeColls.reserve(length);
        _directions.reserve(length);

        for (const auto& n : eColls) {
          if (ServerState::instance()->isRunningInCluster()) {
            auto c = ci->getCollection(_vocbase->name(), n);
            if (!c->isSmart()) {
              addEdgeColl(n, baseDirection);
            } else {
              std::vector<std::string> names;
              names = c->realNamesForRead();
              for (auto const& name : names) {
                addEdgeColl(name, baseDirection);
              }
            }
          } else {
            addEdgeColl(n, baseDirection);
          }
        }
      }
    }
  }

  parseNodeInput(start, _startVertexId, _inStartVariable);
  parseNodeInput(target, _targetVertexId, _inTargetVariable);
}

/// @brief Internal constructor to clone the node.
ShortestPathNode::ShortestPathNode(ExecutionPlan* plan, size_t id,
                                   TRI_vocbase_t* vocbase,
                                   std::vector<std::string> const& edgeColls,
                                   std::vector<TRI_edge_direction_e> const& directions,
                                   Variable const* inStartVariable,
                                   std::string const& startVertexId,
                                   Variable const* inTargetVariable,
                                   std::string const& targetVertexId,
                                   std::unique_ptr<BaseOptions>& options)
    : GraphNode(plan, id, vocbase, options),
      _inStartVariable(inStartVariable),
      _startVertexId(startVertexId),
      _inTargetVariable(inTargetVariable),
      _targetVertexId(targetVertexId),
      _directions(directions),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  _graphInfo.openArray();
  for (auto const& it : edgeColls) {
    _edgeColls.emplace_back(it);
    _graphInfo.add(VPackValue(it));
  }
  _graphInfo.close();
}

ShortestPathNode::~ShortestPathNode() {}

ShortestPathNode::ShortestPathNode(ExecutionPlan* plan,
                                   arangodb::velocypack::Slice const& base)
    : GraphNode(plan, base),
      _inStartVariable(nullptr),
      _inTargetVariable(nullptr),
      _fromCondition(nullptr),
      _toCondition(nullptr) {
  // Directions
  VPackSlice dirList = base.get("directions");
  for (auto const& it : VPackArrayIterator(dirList)) {
    uint64_t dir = arangodb::basics::VelocyPackHelper::stringUInt64(it);
    TRI_edge_direction_e d;
    switch (dir) {
      case 0:
        d = TRI_EDGE_ANY;
        break;
      case 1:
        d = TRI_EDGE_IN;
        break;
      case 2:
        d = TRI_EDGE_OUT;
        break;
      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "Invalid direction value");
        break;
    }
    _directions.emplace_back(d);
  }

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

  std::string graphName;
  if (base.hasKey("graph") && (base.get("graph").isString())) {
    graphName = base.get("graph").copyString();
    if (base.hasKey("graphDefinition")) {
      _graphObj = plan->getAst()->query()->lookupGraphByName(graphName);

      if (_graphObj == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_NOT_FOUND);
      }

      auto const& eColls = _graphObj->edgeCollections();
      for (auto const& it : eColls) {
        _edgeColls.push_back(it);
        
        // if there are twice as many directions as collections, this means we
        // have a shortest path with direction ANY. we must add each collection
        // twice then
        if (_directions.size() == 2 * eColls.size()) {
          // add collection again
          _edgeColls.push_back(it);
        }
      }
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "missing graphDefinition.");
    }
  } else {
    _graphInfo.add(base.get("graph"));
    if (!_graphInfo.slice().isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                     "graph has to be an array.");
    }
    // List of edge collection names
    for (auto const& it : VPackArrayIterator(_graphInfo.slice())) {
      if (!it.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                       "graph has to be an array of strings.");
      }
      _edgeColls.emplace_back(it.copyString());
    }
    if (_edgeColls.empty()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_BAD_JSON_PLAN,
          "graph has to be a non empty array of strings.");
    }
  }

  // Out variables
  if (base.hasKey("vertexOutVariable")) {
    _vertexOutVariable = varFromVPack(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.hasKey("edgeOutVariable")) {
    _edgeOutVariable = varFromVPack(plan->getAst(), base, "edgeOutVariable");
  }

  // Temporary Filter Objects
  TRI_ASSERT(base.hasKey("tmpObjVariable"));
  _tmpObjVariable = varFromVPack(plan->getAst(), base, "tmpObjVariable");

  TRI_ASSERT(base.hasKey("tmpObjVarNode"));
  _tmpObjVarNode = new AstNode(plan->getAst(), base.get("tmpObjVarNode"));

  TRI_ASSERT(base.hasKey("tmpIdNode"));
  _tmpIdNode = new AstNode(plan->getAst(), base.get("tmpIdNode"));

  // Filter Condition Parts
  TRI_ASSERT(base.hasKey("fromCondition"));
  _fromCondition = new AstNode(plan->getAst(), base.get("fromCondition"));

  TRI_ASSERT(base.hasKey("toCondition"));
  _toCondition = new AstNode(plan->getAst(), base.get("toCondition"));


  // Flags
  if (base.hasKey("shortestPathFlags")) {
    _options = std::make_unique<ShortestPathOptions>(
        _plan->getAst()->query()->trx(), base);
  }
}

void ShortestPathNode::toVelocyPackHelper(VPackBuilder& nodes,
                                          bool verbose) const {
  ExecutionNode::toVelocyPackHelperGeneric(nodes,
                                           verbose);  // call base class method
  nodes.add("database", VPackValue(_vocbase->name()));
  nodes.add("graph", _graphInfo.slice());
  nodes.add(VPackValue("directions"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& d : _directions) {
      nodes.add(VPackValue(d));
    }
  }

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

  if (_graphObj != nullptr) {
    nodes.add(VPackValue("graphDefinition"));
    _graphObj->toVelocyPack(nodes, verbose);
  }

  // Out variables
  if (usesVertexOutVariable()) {
    nodes.add(VPackValue("vertexOutVariable"));
    vertexOutVariable()->toVelocyPack(nodes);
  }
  if (usesEdgeOutVariable()) {
    nodes.add(VPackValue("edgeOutVariable"));
    edgeOutVariable()->toVelocyPack(nodes);
  }

  // Filter Conditions

  TRI_ASSERT(_tmpObjVariable != nullptr);
  nodes.add(VPackValue("tmpObjVariable"));
  _tmpObjVariable->toVelocyPack(nodes);

  TRI_ASSERT(_tmpObjVarNode != nullptr);
  nodes.add(VPackValue("tmpObjVarNode"));
  _tmpObjVarNode->toVelocyPack(nodes, verbose);

  TRI_ASSERT(_tmpIdNode != nullptr);
  nodes.add(VPackValue("tmpIdNode"));
  _tmpIdNode->toVelocyPack(nodes, verbose);

  TRI_ASSERT(_fromCondition != nullptr);
  nodes.add(VPackValue("fromCondition"));
  _fromCondition->toVelocyPack(nodes, verbose);

  TRI_ASSERT(_toCondition != nullptr);
  nodes.add(VPackValue("toCondition"));
  _toCondition->toVelocyPack(nodes, verbose);

  nodes.add(VPackValue("shortestPathFlags"));
  _options->toVelocyPack(nodes);

  // And close it:
  nodes.close();
}

ExecutionNode* ShortestPathNode::clone(ExecutionPlan* plan,
                                       bool withDependencies,
                                       bool withProperties) const {
  TRI_ASSERT(!_optionsBuild);
  std::unique_ptr<BaseOptions> tmp =
      std::make_unique<ShortestPathOptions>(*options());
  auto c = new ShortestPathNode(plan, _id, _vocbase, _edgeColls, _directions,
                                _inStartVariable, _startVertexId,
                                _inTargetVariable, _targetVertexId, tmp);
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

double ShortestPathNode::estimateCost(size_t& nrItems) const {
  // Standard estimation for Shortest path is O(|E| + |V|*LOG(|V|))
  // At this point we know |E| but do not know |V|.
  size_t incoming = 0;
  double depCost = _dependencies.at(0)->getCost(incoming);
  auto collections = _plan->getAst()->query()->collections();
  size_t edgesCount = 0;

  TRI_ASSERT(collections != nullptr);

  for (auto const& it : _edgeColls) {
    auto collection = collections->get(it);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "unexpected pointer for collection");
    }
    transaction::Methods* trx = _plan->getAst()->query()->trx();
    edgesCount += collection->count(trx);
  }
  // Hard-Coded number of vertices edges / 10
  nrItems = edgesCount + static_cast<size_t>(std::log2(edgesCount / 10) * (edgesCount / 10));
  return depCost + nrItems;
}


void ShortestPathNode::prepareOptions() {
  if (_optionsBuild) {
    return;
  }
  TRI_ASSERT(!_optionsBuild);
  _options->setVariable(_tmpObjVariable);

  size_t numEdgeColls = _edgeColls.size();
  Ast* ast = _plan->getAst();
  auto opts = options();

  // Compute Indexes.
  for (size_t i = 0; i < numEdgeColls; ++i) {
    auto dir = _directions[i];
    switch (dir) {
      case TRI_EDGE_IN:
        opts->addLookupInfo(
            ast, _edgeColls[i], StaticStrings::ToString,
            _toCondition->clone(ast));
        opts->addReverseLookupInfo(
            ast, _edgeColls[i], StaticStrings::FromString,
            _fromCondition->clone(ast));
        break;
      case TRI_EDGE_OUT:
        opts->addLookupInfo(
            ast, _edgeColls[i], StaticStrings::FromString,
            _fromCondition->clone(ast));
        opts->addReverseLookupInfo(
            ast, _edgeColls[i], StaticStrings::ToString,
            _toCondition->clone(ast));
        break;
      case TRI_EDGE_ANY:
        TRI_ASSERT(false);
        break;
    }
  }
  // If we use the path output the cache should activate document
  // caching otherwise it is not worth it.
  _options->activateCache(false);
  _optionsBuild = true;
}

ShortestPathOptions* ShortestPathNode::options() const {
  auto tmp = static_cast<ShortestPathOptions*>(_options.get());
  TRI_ASSERT(tmp != nullptr);
  return tmp;
}
