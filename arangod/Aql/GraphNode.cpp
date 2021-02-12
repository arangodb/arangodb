////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "GraphNode.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNodeId.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/RegisterPlan.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/SortCondition.h"
#include "Aql/TraversalExecutor.h"
#include "Aql/Variable.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterTraverser.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/Graph.h"
#include "Graph/SingleServerTraverser.h"
#include "Graph/TraverserOptions.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#ifdef USE_ENTERPRISE
#include "Enterprise/Aql/LocalKShortestPathsNode.h"
#include "Enterprise/Aql/LocalShortestPathNode.h"
#include "Enterprise/Aql/LocalTraversalNode.h"
#endif

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>
#include <utility>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::traverser;

namespace {

TRI_edge_direction_e uint64ToDirection(uint64_t dirNum) {
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

TRI_edge_direction_e parseDirection(AstNode const* node) {
  TRI_ASSERT(node->isIntValue() || node->type == NODE_TYPE_DIRECTION);
  AstNode const* dirNode = node;
  if (node->type == NODE_TYPE_DIRECTION) {
    TRI_ASSERT(node->numMembers() == 2);
    dirNode = node->getMember(0);
  }
  TRI_ASSERT(dirNode->isIntValue());
  return uint64ToDirection(dirNode->getIntValue());
}

}

GraphNode::GraphNode(ExecutionPlan* plan, ExecutionNodeId id,
                     TRI_vocbase_t* vocbase, AstNode const* direction,
                     AstNode const* graph, std::unique_ptr<BaseOptions> options)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(parseDirection(direction)),
      _optionsBuilt(false),
      _isSmart(false),
      _isDisjoint(false),
      _options(std::move(options)) {
  // Direction is already the correct Integer.
  // Is not inserted by user but by enum.

  TRI_ASSERT(_vocbase != nullptr);
  TRI_ASSERT(_options != nullptr);

  TRI_ASSERT(direction != nullptr);
  TRI_ASSERT(graph != nullptr);

  auto& ci = _vocbase->server().getFeature<ClusterFeature>().clusterInfo();

  if (graph->type == NODE_TYPE_COLLECTION_LIST) {
    size_t edgeCollectionCount = graph->numMembers();

    _graphInfo.openArray();
    _edgeColls.reserve(edgeCollectionCount);
    _directions.reserve(edgeCollectionCount);

    // First determine whether all edge collections are smart and sharded
    // like a common collection:
    if (ServerState::instance()->isRunningInCluster()) {
      _isSmart = true;
      _isDisjoint = true;
      std::string distributeShardsLike;
      for (size_t i = 0; i < edgeCollectionCount; ++i) {
        auto col = graph->getMember(i);
        if (col->type == NODE_TYPE_DIRECTION) {
          col = col->getMember(1);  // The first member always is the collection
        }
        std::string n = col->getString();
        auto c = ci.getCollection(_vocbase->name(), n);
        if (c->isSmart() && !c->isDisjoint()) {
          _isDisjoint = false;
        }
        if (!c->isSmart() || c->distributeShardsLike().empty()) {
          _isSmart = false;
          _isDisjoint = false;
          break;
        }
        if (distributeShardsLike.empty()) {
          distributeShardsLike = c->distributeShardsLike();
        } else if (distributeShardsLike != c->distributeShardsLike()) {
          _isSmart = false;
          _isDisjoint = false;
          break;
        }
      }
    }

    std::unordered_map<std::string, TRI_edge_direction_e> seenCollections;
    CollectionNameResolver const& resolver = plan->getAst()->query().resolver();

    // List of edge collection names
    for (size_t i = 0; i < edgeCollectionCount; ++i) {
      auto col = graph->getMember(i);
      TRI_edge_direction_e dir = TRI_EDGE_ANY;

      if (col->type == NODE_TYPE_DIRECTION) {
        // We have a collection with special direction.
        dir = parseDirection(col->getMember(0));
        col = col->getMember(1);
      } else {
        dir = _defaultDirection;
      }

      std::string eColName = col->getString();

      if (_options->shouldExcludeEdgeCollection(eColName)) {
        // excluded edge collection
        continue;
      }

      // now do some uniqueness checks for the specified collections
      auto [it, inserted] = seenCollections.try_emplace(eColName, dir);
      if (!inserted) {
        if ((*it).second != dir) {
          std::string msg("conflicting directions specified for collection '" +
                          std::string(eColName));
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID, msg);
        }
        // do not re-add the same collection!
        continue;
      }

      auto collection = resolver.getCollection(eColName);

      if (!collection || collection->type() != TRI_COL_TYPE_EDGE) {
        std::string msg("collection type invalid for collection '" + std::string(eColName) +
                        ": expecting collection type 'edge'");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID, msg);
      }

      auto& collections = plan->getAst()->query().collections();

      _graphInfo.add(VPackValue(eColName));
      if (ServerState::instance()->isRunningInCluster()) {
        auto c = ci.getCollection(_vocbase->name(), eColName);
        if (!c->isSmart()) {
          addEdgeCollection(collections, eColName, dir);
        } else {
          std::vector<std::string> names;
          if (_isSmart) {
            names = c->realNames();
          } else {
            names = c->realNamesForRead();
          }
          for (auto const& name : names) {
            addEdgeCollection(collections, name, dir);
          }
        }
      } else {
        addEdgeCollection(collections, eColName, dir);
      }
    }
    _graphInfo.close();
  } else if (graph->isStringValue()) {
    std::string graphName = graph->getString();
    _graphInfo.add(VPackValue(graphName));
    auto graphLookupResult = plan->getAst()->query().lookupGraphByName(graphName);

    if (graphLookupResult.fail()) {
      THROW_ARANGO_EXCEPTION(graphLookupResult.result());
    }

    TRI_ASSERT(graphLookupResult.ok());
    _graphObj = graphLookupResult.get();

    auto eColls = _graphObj->edgeCollections();
    size_t length = eColls.size();
    if (length == 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
    }

    // First determine whether all edge collections are smart and sharded
    // like a common collection:
    if (ServerState::instance()->isRunningInCluster()) {
      _isSmart = _graphObj->isSmart();
      _isDisjoint = _graphObj->isDisjoint();
    }

    for (const auto& n : eColls) {
      if (_options->shouldExcludeEdgeCollection(n)) {
        // excluded edge collection
        continue;
      }

      auto& collections = plan->getAst()->query().collections();
      if (ServerState::instance()->isRunningInCluster()) {
        auto c = ci.getCollection(_vocbase->name(), n);
        if (!c->isSmart()) {
          addEdgeCollection(collections, n, _defaultDirection);
        } else {
          std::vector<std::string> names;
          if (_isSmart) {
            names = c->realNames();
          } else {
            names = c->realNamesForRead();
          }
          for (auto const& name : names) {
            addEdgeCollection(collections, name, _defaultDirection);
          }
        }
      } else {
        addEdgeCollection(collections, n, _defaultDirection);
      }
    }

    auto& collections = plan->getAst()->query().collections();
    auto vColls = _graphObj->vertexCollections();
    length = vColls.size();
    if (length == 0) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_GRAPH_EMPTY);
    }
    _vertexColls.reserve(length);
    for (auto const& v : vColls) {
      addVertexCollection(collections, v);
    }
  }
}

GraphNode::GraphNode(ExecutionPlan* plan, arangodb::velocypack::Slice const& base)
    : ExecutionNode(plan, base),
      _vocbase(&(plan->getAst()->query().vocbase())),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(nullptr),
      _tmpObjVariable(nullptr),
      _tmpObjVarNode(nullptr),
      _tmpIdNode(nullptr),
      _defaultDirection(uint64ToDirection(arangodb::basics::VelocyPackHelper::stringUInt64(
          base.get("defaultDirection")))),
      _optionsBuilt(false),
      _isSmart(arangodb::basics::VelocyPackHelper::getBooleanValue(base, "isSmart", false)),
      _isDisjoint(arangodb::basics::VelocyPackHelper::getBooleanValue(base, "isDisjoint", false)) {

  if (!ServerState::instance()->isDBServer()) {
    // Graph Information. Do we need to reload the graph here?
    std::string graphName;
    if (base.get("graph").isString()) {
      graphName = base.get("graph").copyString();
      if (base.hasKey("graphDefinition")) {
        // load graph and store pointer
        auto graphLookupResult = plan->getAst()->query().lookupGraphByName(graphName);

        if (graphLookupResult.fail()) {
          THROW_ARANGO_EXCEPTION(graphLookupResult.result());
        }

        _graphObj = graphLookupResult.get();
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
    }
  }

  // Collection information
  VPackSlice edgeCollections = base.get("edgeCollections");
  if (!edgeCollections.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph needs an array of edge collections.");
  }
  // Directions
  VPackSlice dirList = base.get("directions");
  if (!dirList.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph needs an array of directions.");
  }

  if (edgeCollections.length() != dirList.length()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs the same number of edge collections and directions.");
  }

  QueryContext& query = plan->getAst()->query();
  // MSVC did throw an internal compiler error with auto instead of std::string
  // here at the time of this writing (some MSVC 2019 14.25.28610). Could
  // reproduce it only in our CI, my local MSVC (same version) ran fine...
  auto getAqlCollectionFromName = [&](std::string const& name) -> aql::Collection& {
    // if the collection was already existent in the query, addCollection will
    // just return it.
    return *query.collections().add(name, AccessMode::Type::READ, aql::Collection::Hint::Collection);
  };

  auto vPackDirListIter = VPackArrayIterator(dirList);
  auto vPackEdgeColIter = VPackArrayIterator(edgeCollections);
  for (auto dirIt = vPackDirListIter.begin(), edgeIt = vPackEdgeColIter.begin();
       dirIt != vPackDirListIter.end() && edgeIt != vPackEdgeColIter.end();
       ++dirIt, ++edgeIt) {
    uint64_t dir = arangodb::basics::VelocyPackHelper::stringUInt64(*dirIt);
    TRI_edge_direction_e d = uint64ToDirection(dir);
    // Only TRI_EDGE_IN and TRI_EDGE_OUT allowed here
    TRI_ASSERT(d == TRI_EDGE_IN || d == TRI_EDGE_OUT);
    std::string e =
        arangodb::basics::VelocyPackHelper::getStringValue(*edgeIt, "");
    auto& aqlCollection = getAqlCollectionFromName(e);
    addEdgeCollection(aqlCollection, d);
  }

  VPackSlice vertexCollections = base.get("vertexCollections");

  if (!vertexCollections.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs an array of vertex collections.");
  }

  for (VPackSlice it : VPackArrayIterator(vertexCollections)) {
    std::string v = arangodb::basics::VelocyPackHelper::getStringValue(it, "");
    auto& aqlCollection = getAqlCollectionFromName(v);
    addVertexCollection(aqlCollection);
  }

  // translations for one-shard-databases
  VPackSlice collectionToShard = base.get("collectionToShard");
  if (!collectionToShard.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_BAD_JSON_PLAN,
        "graph needs a translation from collection to shard names");
  }
  for (auto const& item : VPackObjectIterator(collectionToShard)) {
    _collectionToShard.insert({item.key.copyString(), item.value.copyString()});
  }

  // Out variables
  if (base.hasKey("vertexOutVariable")) {
    _vertexOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "vertexOutVariable");
  }
  if (base.hasKey("edgeOutVariable")) {
    _edgeOutVariable =
        Variable::varFromVPack(plan->getAst(), base, "edgeOutVariable");
  }

  // Temporary Filter Objects
  TRI_ASSERT(base.hasKey("tmpObjVariable"));
  _tmpObjVariable =
      Variable::varFromVPack(plan->getAst(), base, "tmpObjVariable");

  TRI_ASSERT(base.hasKey("tmpObjVarNode"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _tmpObjVarNode = plan->getAst()->createNode(base.get("tmpObjVarNode"));

  TRI_ASSERT(base.hasKey("tmpIdNode"));
  // the plan's AST takes ownership of the newly created AstNode, so this is
  // safe cppcheck-suppress *
  _tmpIdNode = plan->getAst()->createNode(base.get("tmpIdNode"));

  VPackSlice opts = base.get("options");
  if (!opts.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_BAD_JSON_PLAN,
                                   "graph options have to be a json-object.");
  }

  _options = BaseOptions::createOptionsFromSlice(_plan->getAst()->query(), opts);
  // set traversal-translations
  _options->setCollectionToShard(_collectionToShard);  // could be moved as it will only be used here
  if (_options->parallelism() > 1) {
    _plan->getAst()->setContainsParallelNode();
  }
}

/// @brief Internal constructor to clone the node.
GraphNode::GraphNode(ExecutionPlan* plan, ExecutionNodeId id, TRI_vocbase_t* vocbase,
                     std::vector<Collection*> const& edgeColls,
                     std::vector<Collection*> const& vertexColls,
                     TRI_edge_direction_e defaultDirection,
                     std::vector<TRI_edge_direction_e> directions,
                     std::unique_ptr<graph::BaseOptions> options, Graph const* graph)
    : ExecutionNode(plan, id),
      _vocbase(vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(graph),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(defaultDirection),
      _optionsBuilt(false),
      _isSmart(false),
      _isDisjoint(false),
      _directions(std::move(directions)),
      _options(std::move(options)) {
  setGraphInfoAndCopyColls(edgeColls, vertexColls);
}

void GraphNode::setGraphInfoAndCopyColls(std::vector<Collection*> const& edgeColls,
                                         std::vector<Collection*> const& vertexColls) {
  _graphInfo.openArray();
  for (auto& it : edgeColls) {
    _edgeColls.emplace_back(it);
    _graphInfo.add(VPackValue(it->name()));
  }
  _graphInfo.close();

  for (auto& it : vertexColls) {
    addVertexCollection(*it);
  }
}

GraphNode::GraphNode(ExecutionPlan& plan, GraphNode const& other,
                     std::unique_ptr<graph::BaseOptions> options)
    : ExecutionNode(plan, other),
      _vocbase(other._vocbase),
      _vertexOutVariable(nullptr),
      _edgeOutVariable(nullptr),
      _graphObj(other.graph()),
      _tmpObjVariable(_plan->getAst()->variables()->createTemporaryVariable()),
      _tmpObjVarNode(_plan->getAst()->createNodeReference(_tmpObjVariable)),
      _tmpIdNode(_plan->getAst()->createNodeValueString("", 0)),
      _defaultDirection(other._defaultDirection),
      _optionsBuilt(false),
      _isSmart(other.isSmart()),
      _isDisjoint(other.isDisjoint()),
      _directions(other._directions),
      _options(std::move(options)),
      _collectionToShard(other._collectionToShard) {
  setGraphInfoAndCopyColls(other.edgeColls(), other.vertexColls());
}

GraphNode::GraphNode(THIS_THROWS_WHEN_CALLED)
    : ExecutionNode(nullptr, ExecutionNodeId{0}), _defaultDirection() {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

std::string const& GraphNode::collectionToShardName(std::string const& collName) const {
  if (_collectionToShard.empty()) {
    return collName;
  }

  auto found = _collectionToShard.find(collName);
  if (found == _collectionToShard.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL_AQL, "unable to find shard '" + collName + "' in query shard map");
  }
  return found->second;
}

void GraphNode::toVelocyPackHelper(VPackBuilder& nodes, unsigned flags,
                                   std::unordered_set<ExecutionNode const*>& seen) const {
  // call base class method
  ExecutionNode::toVelocyPackHelperGeneric(nodes, flags, seen);

  // Vocbase
  nodes.add("database", VPackValue(_vocbase->name()));

  // TODO We need Both?!
  // Graph definition
  nodes.add("graph", _graphInfo.slice());
  nodes.add("isLocalGraphNode", VPackValue(isLocalGraphNode()));
  nodes.add("isUsedAsSatellite", VPackValue(isUsedAsSatellite()));

  // Graph Definition
  if (_graphObj != nullptr) {
    nodes.add(VPackValue("graphDefinition"));
    _graphObj->toVelocyPack(nodes);
  }

  // Default Direction
  nodes.add("defaultDirection", VPackValue(_defaultDirection));

  // Directions
  nodes.add(VPackValue("directions"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& d : _directions) {
      nodes.add(VPackValue(d));
    }
  }

  // Collections
  nodes.add(VPackValue("edgeCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& e : _edgeColls) {
      auto const& shard = collectionToShardName(e->name());
      // if the mapped shard for a collection is empty, it means that
      // we have an edge collection that is only relevant on some of the
      // target servers
      if (!shard.empty()) {
        nodes.add(VPackValue(shard));
      }
    }
  }

  nodes.add(VPackValue("vertexCollections"));
  {
    VPackArrayBuilder guard(&nodes);
    for (auto const& v : _vertexColls) {
      // if the mapped shard for a collection is empty, it means that
      // we have a vertex collection that is only relevant on some of the
      // target servers
      auto const& shard = collectionToShardName(v->name());
      if (!shard.empty()) {
        nodes.add(VPackValue(shard));
      }
    }
  }

  // translations for one-shard-databases
  nodes.add(VPackValue("collectionToShard"));
  {
    VPackObjectBuilder guard(&nodes);
    for (auto const& item : _collectionToShard) {
      nodes.add(item.first, VPackValue(item.second));
    }
  }

  // Out variables
  if (isVertexOutVariableUsedLater()) {
    nodes.add(VPackValue("vertexOutVariable"));
    vertexOutVariable()->toVelocyPack(nodes);
  }
  if (isEdgeOutVariableUsedLater()) {
    nodes.add(VPackValue("edgeOutVariable"));
    edgeOutVariable()->toVelocyPack(nodes);
  }

  // Flags
  nodes.add("isSmart", VPackValue(_isSmart));
  nodes.add("isDisjoint", VPackValue(_isDisjoint));

  // Temporary AST Nodes for conditions
  TRI_ASSERT(_tmpObjVariable != nullptr);
  nodes.add(VPackValue("tmpObjVariable"));
  _tmpObjVariable->toVelocyPack(nodes);

  TRI_ASSERT(_tmpObjVarNode != nullptr);
  nodes.add(VPackValue("tmpObjVarNode"));
  _tmpObjVarNode->toVelocyPack(nodes, flags != 0);

  TRI_ASSERT(_tmpIdNode != nullptr);
  nodes.add(VPackValue("tmpIdNode"));
  _tmpIdNode->toVelocyPack(nodes, flags != 0);

  nodes.add(VPackValue("options"));
  _options->toVelocyPack(nodes);

  nodes.add(VPackValue("indexes"));
  _options->toVelocyPackIndexes(nodes);
}

CostEstimate GraphNode::estimateCost() const {
  CostEstimate estimate = _dependencies.at(0)->getCost();
  size_t incoming = estimate.estimatedNrItems;
  if (_optionsBuilt) {
    // We know which indexes are in use.
    estimate.estimatedCost += incoming * _options->estimateCost(estimate.estimatedNrItems);
  } else {
    // Some hard-coded value, this is identical to build lookupInfos
    // if no index estimate is availble (and it is not as long as the options are not built)
    double baseCost = 1;
    size_t baseNumItems = 0;
    for (auto& e : _edgeColls) {
      auto count = e->count(_options->trx(), transaction::CountType::TryCache);
      // Assume an estimate if 10% hit rate
      baseCost *= count / 10;
      baseNumItems += static_cast<size_t>(std::ceil(count / 10));
    }

    size_t estDepth = _options->estimateDepth();
    double tmpNrItems = incoming * std::pow(baseNumItems, estDepth);
    // Protect against size_t overflow, just to be sure.
    if (tmpNrItems > static_cast<double>(std::numeric_limits<size_t>::max())) {
      // This will be an expensive query...
      estimate.estimatedNrItems = std::numeric_limits<size_t>::max();
    } else {
      estimate.estimatedNrItems += static_cast<size_t>(tmpNrItems);
    }
    estimate.estimatedCost += incoming * std::pow(baseCost, estDepth);
  }

  return estimate;
}

void GraphNode::addEngine(aql::EngineId engineId, ServerID const& server) {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  _engines.try_emplace(server, engineId);
}

/// @brief Returns a reference to the engines. (CLUSTER ONLY)
std::unordered_map<ServerID, aql::EngineId> const* GraphNode::engines() const {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  return &_engines;
}

/// @brief Clears the graph Engines. (CLUSTER ONLY)
/// NOTE: Use with care, if you do not refill
/// the engines this graph node cannot communicate.
/// and will yield no results.
void GraphNode::clearEngines() {
  TRI_ASSERT(arangodb::ServerState::instance()->isCoordinator());
  _engines.clear();
}

BaseOptions* GraphNode::options() const { return _options.get(); }

AstNode* GraphNode::getTemporaryRefNode() const { return _tmpObjVarNode; }

Variable const* GraphNode::getTemporaryVariable() const {
  return _tmpObjVariable;
}

void GraphNode::getConditionVariables(std::vector<Variable const*>& res) const {
  // No variables used, nothing todo
}

Collection const* GraphNode::collection() const {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  TRI_ASSERT(!_edgeColls.empty());
  TRI_ASSERT(_edgeColls.front() != nullptr);
  return _edgeColls.front();
}

void GraphNode::injectVertexCollection(aql::Collection& other) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // This is a workaround to inject all unknown aql collections into
  // this node, that should be list of unique values!
  for (auto const& v : _vertexColls) {
    TRI_ASSERT(v->name() != other.name());
  }
#endif
  addVertexCollection(other);
}

#ifndef USE_ENTERPRISE
void GraphNode::enhanceEngineInfo(VPackBuilder& builder) const {
  if (_graphObj != nullptr) {
    _graphObj->enhanceEngineInfo(builder);
  } else {
    // TODO enhance the Info based on EdgeCollections.
  }
}
#endif

void GraphNode::addEdgeCollection(Collections const& collections, std::string const& name,
                                  TRI_edge_direction_e dir) {
  auto aqlCollection = collections.get(name);
  if (aqlCollection != nullptr) {
    addEdgeCollection(*aqlCollection, dir);
  } else {
    std::string msg = "Edge collection `";
    msg += name;
    msg += "` could not be found";
    if (_graphObj != nullptr) {
      msg += ", but is part of graph `";
      msg += _graphObj->name();
      msg += "`";
    }
    msg += ".";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg);
  }
}

void GraphNode::addEdgeCollection(aql::Collection& collection, TRI_edge_direction_e dir) {
  auto const& n = collection.name();
  if (_isSmart) {
    if (n.compare(0, 6, "_from_") == 0) {
      if (dir != TRI_EDGE_IN) {
        _directions.emplace_back(TRI_EDGE_OUT);
        _edgeColls.emplace_back(&collection);
      }
      return;
    } else if (n.compare(0, 4, "_to_") == 0) {
      if (dir != TRI_EDGE_OUT) {
        _directions.emplace_back(TRI_EDGE_IN);
        _edgeColls.emplace_back(&collection);
      }
      return;
    }
  }

  if (dir == TRI_EDGE_ANY) {
    _directions.emplace_back(TRI_EDGE_OUT);
    _edgeColls.emplace_back(&collection);

    _directions.emplace_back(TRI_EDGE_IN);
    _edgeColls.emplace_back(&collection);
  } else {
    _directions.emplace_back(dir);
    _edgeColls.emplace_back(&collection);
  }
}

void GraphNode::addVertexCollection(Collections const& collections, std::string const& name) {
  auto aqlCollection = collections.get(name);
  if (aqlCollection != nullptr) {
    addVertexCollection(*aqlCollection);
  } else {
    std::string msg = "Vertex collection `";
    msg += name;
    msg += "` could not be found";
    // If we have vertex collections, _graphObj should never be nullptr.
    // But let's stay on the side that's obviously safe without any context.
    if (_graphObj != nullptr) {
      msg += ", but is part of graph `";
      msg += _graphObj->name();
      msg += "`";
    }
    msg += ".";
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND, msg);
  }
}

#ifndef USE_ENTERPRISE
void GraphNode::addVertexCollection(aql::Collection& collection) {
  _vertexColls.emplace_back(&collection);
}
#endif

std::vector<aql::Collection const*> GraphNode::collections() const {
  std::unordered_set<aql::Collection const*> set;
  set.reserve(_edgeColls.size() + _vertexColls.size());

  for (auto const& collPointer : _edgeColls) {
    set.emplace(collPointer);
  }
  for (auto const& collPointer : _vertexColls) {
    set.emplace(collPointer);
  }

  std::vector<aql::Collection const*> vector;
  vector.reserve(set.size());
  std::move(set.begin(), set.end(), std::back_inserter(vector));

  return vector;
}

bool GraphNode::isSmart() const { return _isSmart; }

bool GraphNode::isDisjoint() const { return _isDisjoint; }

TRI_vocbase_t* GraphNode::vocbase() const { return _vocbase; }

Variable const* GraphNode::vertexOutVariable() const {
  return _vertexOutVariable;
}

bool GraphNode::isVertexOutVariableUsedLater() const {
  return _vertexOutVariable != nullptr && _options->produceVertices();
}

void GraphNode::setVertexOutput(Variable const* outVar) {
  _vertexOutVariable = outVar;
}

Variable const* GraphNode::edgeOutVariable() const { return _edgeOutVariable; }

bool GraphNode::isEdgeOutVariableUsedLater() const {
  return _edgeOutVariable != nullptr && _options->produceEdges();
}

void GraphNode::setEdgeOutput(Variable const* outVar) {
  _edgeOutVariable = outVar;
}

std::vector<aql::Collection*> const& GraphNode::edgeColls() const {
  return _edgeColls;
}

std::vector<aql::Collection*> const& GraphNode::vertexColls() const {
  return _vertexColls;
}

graph::Graph const* GraphNode::graph() const noexcept { return _graphObj; }

bool GraphNode::isEligibleAsSatelliteTraversal() const {
  return graph() != nullptr && graph()->isSatellite();
}

/* Enterprise features */

#ifndef USE_ENTERPRISE

bool GraphNode::isUsedAsSatellite() const { return false; }

bool GraphNode::isLocalGraphNode() const { return false; }

void GraphNode::waitForSatelliteIfRequired(ExecutionEngine const* engine) const {}

#endif
