////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "EngineInfoContainerDBServer.h"

#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Query.h"
#include "Basics/Result.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "Cluster/TraverserEngineRegistry.h"
#include "Graph/BaseOptions.h"
#include "StorageEngine/TransactionState.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::aql;
static const double SETUP_TIMEOUT = 25.0;

EngineInfoContainerDBServer::EngineInfo::EngineInfo(size_t idOfRemoteNode)
    : _idOfRemoteNode(idOfRemoteNode), _otherId(0), _collection(nullptr) {}

EngineInfoContainerDBServer::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes
  // they are managed by the AST somewhere else
  // We are also not responsible for the collection.
  TRI_ASSERT(!_nodes.empty());
}

EngineInfoContainerDBServer::EngineInfo::EngineInfo(EngineInfo const&& other)
    : _nodes(std::move(other._nodes)),
      _idOfRemoteNode(other._idOfRemoteNode),
      _otherId(other._otherId),
      _collection(other._collection) {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(_collection != nullptr);
}

void EngineInfoContainerDBServer::EngineInfo::connectQueryId(QueryId id) {
  _otherId = id;
}

void EngineInfoContainerDBServer::EngineInfo::addNode(ExecutionNode* node) {
  _nodes.emplace_back(node);
}

Collection const* EngineInfoContainerDBServer::EngineInfo::collection() const {
  return _collection;
}

void EngineInfoContainerDBServer::EngineInfo::collection(Collection* col) {
  _collection = col;
}

void EngineInfoContainerDBServer::EngineInfo::serializeSnippet(
    Query* query, ShardID id, VPackBuilder& infoBuilder,
    bool isResponsibleForInit) const {
  // The Key is required to build up the queryId mapping later
  infoBuilder.add(VPackValue(
      arangodb::basics::StringUtils::itoa(_idOfRemoteNode) + ":" + id));

  TRI_ASSERT(!_nodes.empty());
  // copy the relevant fragment of the plan for each shard
  // Note that in these parts of the query there are no SubqueryNodes,
  // since they are all on the coordinator!
  // Also note: As _collection is set to the correct current shard
  // this clone does the translation collection => shardId implicitly
  // at the relevant parts of the query.

  _collection->setCurrentShard(id);

  ExecutionPlan plan(query->ast());
  ExecutionNode* previous = nullptr;

  // for (ExecutionNode const* current : _nodes) {
  for (auto enIt = _nodes.rbegin(); enIt != _nodes.rend(); ++enIt) {
    ExecutionNode const* current = *enIt;
    auto clone = current->clone(&plan, false, false);
    // UNNECESSARY, because clone does it: plan.registerNode(clone);

    if (current->getType() == ExecutionNode::REMOTE) {
      auto rem = static_cast<RemoteNode*>(clone);
      // update the remote node with the information about the query
      rem->server("server:" + arangodb::ServerState::instance()->getId());
      rem->ownName(id);
      rem->queryId(_otherId);

      // only one of the remote blocks is responsible for forwarding the
      // initializeCursor and shutDown requests
      // for simplicity, we always use the first remote block if we have more
      // than one

      // Do we still need this???
      rem->isResponsibleForInitializeCursor(isResponsibleForInit);
    }

    if (previous != nullptr) {
      clone->addDependency(previous);
    }

    previous = clone;
  }
  TRI_ASSERT(previous != nullptr);

  plan.root(previous);
  plan.setVarUsageComputed();
  // Always Verbose
  plan.root()->toVelocyPack(infoBuilder, true);
  _collection->resetCurrentShard();
}

EngineInfoContainerDBServer::EngineInfoContainerDBServer() {}

EngineInfoContainerDBServer::~EngineInfoContainerDBServer() {}

void EngineInfoContainerDBServer::addNode(ExecutionNode* node) {
  TRI_ASSERT(!_engineStack.empty());
  _engineStack.top()->addNode(node);
  switch (node->getType()) {
    case ExecutionNode::ENUMERATE_COLLECTION:
      handleCollection(
          static_cast<EnumerateCollectionNode*>(node)->collection(),
          AccessMode::Type::READ, true);
      break;
    case ExecutionNode::INDEX:
      handleCollection(static_cast<IndexNode*>(node)->collection(),
                       AccessMode::Type::READ, true);
      break;
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT:
      handleCollection(static_cast<ModificationNode*>(node)->collection(),
                       AccessMode::Type::WRITE, true);
      break;
    default:
      // Do nothing
      break;
  };
}

void EngineInfoContainerDBServer::openSnippet(size_t idOfRemoteNode) {
  _engineStack.emplace(std::make_shared<EngineInfo>(idOfRemoteNode));
}

// Closing a snippet means:
// 1. pop it off the stack.
// 2. Wire it up with the given coordinator ID
// 3. Move it in the Collection => Engine map

void EngineInfoContainerDBServer::closeSnippet(QueryId coordinatorEngineId) {
  TRI_ASSERT(!_engineStack.empty());
  auto e = _engineStack.top();
  _engineStack.pop();

  e->connectQueryId(coordinatorEngineId);
  TRI_ASSERT(e->collection() != nullptr);
  auto& engine = _engines[e->collection()];
  engine.emplace_back(std::move(e));
}

// This first defines the lock required for this collection
// Then we update the collection pointer of the last engine.

#ifndef USE_ENTERPRISE
void EngineInfoContainerDBServer::handleCollection(
    Collection const* col, AccessMode::Type const& accessType,
    bool updateCollection) {
  auto it = _collections.find(col);
  if (it == _collections.end()) {
    _collections.emplace(col, accessType);
  } else {
    if (it->second < accessType) {
      // We need to upgrade the lock
      it->second = accessType;
    }
  }
  if (updateCollection) {
    TRI_ASSERT(!_engineStack.empty());
    auto e = _engineStack.top();
    // ... const_cast
    e->collection(const_cast<Collection*>(col));
  }
}
#endif

EngineInfoContainerDBServer::DBServerInfo::DBServerInfo() {}
EngineInfoContainerDBServer::DBServerInfo::~DBServerInfo() {}

void EngineInfoContainerDBServer::DBServerInfo::addShardLock(
    AccessMode::Type const& lock, ShardID const& id) {
  _shardLocking[lock].emplace_back(id);
}

void EngineInfoContainerDBServer::DBServerInfo::addEngine(
    std::shared_ptr<EngineInfoContainerDBServer::EngineInfo> info,
    ShardID const& id) {
  _engineInfos[info].emplace_back(id);
}

void EngineInfoContainerDBServer::DBServerInfo::buildMessage(
    Query* query, VPackBuilder& infoBuilder) const {
  TRI_ASSERT(infoBuilder.isEmpty());

  infoBuilder.openObject();
  infoBuilder.add(VPackValue("lockInfo"));
  infoBuilder.openObject();
  for (auto const& shardLocks : _shardLocking) {
    infoBuilder.add(VPackValue(AccessMode::typeString(shardLocks.first)));
    infoBuilder.openArray();
    for (auto const& s : shardLocks.second) {
      infoBuilder.add(VPackValue(s));
    }
    infoBuilder.close();  // The array
  }
  infoBuilder.close();  // lockInfo
  infoBuilder.add(VPackValue("options"));
  injectQueryOptions(query, infoBuilder);
  infoBuilder.add(VPackValue("variables"));
  // This will open and close an Object.
  query->ast()->variables()->toVelocyPack(infoBuilder);
  infoBuilder.add(VPackValue("snippets"));
  infoBuilder.openObject();

  for (auto const& it : _engineInfos) {
    bool isResponsibleForInit = true;
    for (auto const& s : it.second) {
      it.first->serializeSnippet(query, s, infoBuilder, isResponsibleForInit);
      isResponsibleForInit = false;
    }
  }
  infoBuilder.close();  // snippets
  injectTraverserEngines(infoBuilder);
  infoBuilder.close();  // Object
}

void EngineInfoContainerDBServer::DBServerInfo::injectTraverserEngines(
    VPackBuilder& infoBuilder) const {
  if (_traverserEngineInfos.empty()) {
    return;
  }
  TRI_ASSERT(infoBuilder.isOpenObject());
  infoBuilder.add(VPackValue("traverserEngines"));
  infoBuilder.openArray();
  for (auto const& it : _traverserEngineInfos) {
    GraphNode* en = it.first;
    auto const& list = it.second;
    infoBuilder.openObject();
    {
      // Options
      infoBuilder.add(VPackValue("options"));
      graph::BaseOptions* opts = en->options();
      opts->buildEngineInfo(infoBuilder);
    }
    {
      // Variables
      std::vector<aql::Variable const*> vars;
      en->getConditionVariables(vars);
      if (!vars.empty()) {
        infoBuilder.add(VPackValue("variables"));
        infoBuilder.openArray();
        for (auto v : vars) {
          v->toVelocyPack(infoBuilder);
        }
        infoBuilder.close();
      }
    }

    infoBuilder.add(VPackValue("shards"));
    infoBuilder.openObject();
    infoBuilder.add(VPackValue("vertices"));
    infoBuilder.openObject();
    for (auto const& col : list.vertexCollections) {
      infoBuilder.add(VPackValue(col.first));
      infoBuilder.openArray();
      for (auto const& v : col.second) {
        infoBuilder.add(VPackValue(v));
      }
      infoBuilder.close();  // this collection
    }
    infoBuilder.close();  // vertices

    infoBuilder.add(VPackValue("edges"));
    infoBuilder.openArray();
    for (auto const& edgeShards : list.edgeCollections) {
      infoBuilder.openArray();
      for (auto const& e : edgeShards) {
        infoBuilder.add(VPackValue(e));
      }
      infoBuilder.close();
    }
    infoBuilder.close();  // edges
    infoBuilder.close();  // shards

    en->enhanceEngineInfo(infoBuilder);

    infoBuilder.close();  // base
  }

  infoBuilder.close();  // traverserEngines
}

void EngineInfoContainerDBServer::DBServerInfo::combineTraverserEngines(
    ServerID const& serverID, VPackSlice const ids) {
  if (ids.length() != _traverserEngineInfos.size()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                                   "The DBServer was not able to create enough "
                                   "traversal engines. This can happen during "
                                   "failover. Please check; " +
                                       serverID);
  }
  auto idIter = VPackArrayIterator(ids);
  // We need to use the same order of iterating over
  // the traverserEngineInfos to wire the correct GraphNodes
  // to the correct engine ids
  for (auto const& it : _traverserEngineInfos) {
    it.first->addEngine(
        idIter.value().getNumber<traverser::TraverserEngineID>(), serverID);
    idIter.next();
  }
}

void EngineInfoContainerDBServer::DBServerInfo::addTraverserEngine(
    GraphNode* node, TraverserEngineShardLists&& shards) {
  _traverserEngineInfos.push_back(std::make_pair(node, std::move(shards)));
}

void EngineInfoContainerDBServer::DBServerInfo::injectQueryOptions(
    Query* query, VPackBuilder& infoBuilder) const {
  // the toVelocyPack will open & close the "options" object
  query->queryOptions().toVelocyPack(infoBuilder, true);
}

std::map<ServerID, EngineInfoContainerDBServer::DBServerInfo>
EngineInfoContainerDBServer::createDBServerMapping(
    std::unordered_set<std::string> const& restrictToShards,
    std::unordered_set<ShardID>& lockedShards) const {
  auto ci = ClusterInfo::instance();

  std::map<ServerID, DBServerInfo> dbServerMapping;

  for (auto const& it : _collections) {
    // it.first => Collection const*
    // it.second => Lock Type
    std::vector<std::shared_ptr<EngineInfo>> const* engines = nullptr;
    if (_engines.find(it.first) != _engines.end()) {
      engines = &_engines.find(it.first)->second;
    }
    auto shardIds = it.first->shardIds(restrictToShards);
    for (auto const& s : *(shardIds.get())) {
      lockedShards.emplace(s);
      auto const servers = ci->getResponsibleServer(s);
      if (servers == nullptr || servers->empty()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
            "Could not find responsible server for shard " + s);
      }
      auto responsible = servers->at(0);
      auto& mapping = dbServerMapping[responsible];
      mapping.addShardLock(it.second, s);
      if (engines != nullptr) {
        for (auto& e : *engines) {
          mapping.addEngine(e, s);
        }
      }
    }
  }

#ifdef USE_ENTERPRISE
  prepareSatellites(dbServerMapping, restrictToShards);
#endif

  return dbServerMapping;
}

void EngineInfoContainerDBServer::injectGraphNodesToMapping(
    Query* query, std::unordered_set<std::string> const& restrictToShards,
    std::map<ServerID, EngineInfoContainerDBServer::DBServerInfo>&
        dbServerMapping) const {
  if (_graphNodes.empty()) {
    return;
  }

#ifdef USE_ENTERPRISE
  transaction::Methods* trx = query->trx();
  transaction::Options& trxOps = query->trx()->state()->options();
#endif

  auto clusterInfo = arangodb::ClusterInfo::instance();

  /// Typedef for a complicated mapping used in TraverserEngines.
  typedef std::unordered_map<
      ServerID, EngineInfoContainerDBServer::TraverserEngineShardLists>
      Serv2ColMap;

  for (GraphNode* en : _graphNodes) {
    // Every node needs it's own Serv2ColMap
    Serv2ColMap mappingServerToCollections;
    en->prepareOptions();

    std::vector<std::unique_ptr<arangodb::aql::Collection>> const& edges =
        en->edgeColls();

    // Here we create a mapping
    // ServerID => ResponsibleShards
    // Where Responsible shards is divided in edgeCollections and
    // vertexCollections
    // For edgeCollections the Ordering is important for the index access.
    // Also the same edgeCollection can be included twice (iff direction is ANY)
    size_t length = edges.size();

    auto findServerLists = [&](ShardID const& shard) -> Serv2ColMap::iterator {
      auto serverList = clusterInfo->getResponsibleServer(shard);
      if (serverList->empty()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
            "Could not find responsible server for shard " + shard);
      }
      TRI_ASSERT(!serverList->empty());
      auto& leader = (*serverList)[0];
      auto pair = mappingServerToCollections.find(leader);
      if (pair == mappingServerToCollections.end()) {
        mappingServerToCollections.emplace(leader,
                                           TraverserEngineShardLists{length});
        pair = mappingServerToCollections.find(leader);
      }
      return pair;
    };

    for (size_t i = 0; i < length; ++i) {
      auto shardIds = edges[i]->shardIds(restrictToShards);
      for (auto const& shard : *shardIds) {
        auto pair = findServerLists(shard);
        pair->second.edgeCollections[i].emplace_back(shard);
      }
    }

    std::vector<std::unique_ptr<arangodb::aql::Collection>> const& vertices =
        en->vertexColls();
    if (vertices.empty()) {
      std::unordered_set<std::string> knownEdges;
      for (auto const& it : edges) {
        knownEdges.emplace(it->getName());
      }
      // This case indicates we do not have a named graph. We simply use
      // ALL collections known to this query.
      std::map<std::string, Collection*>* cs =
          query->collections()->collections();
      for (auto const& collection : (*cs)) {
        if (knownEdges.find(collection.second->getName()) == knownEdges.end()) {
          // This collection is not one of the edge collections used in this
          // graph.
          auto shardIds = collection.second->shardIds(restrictToShards);
          for (ShardID const& shard : *shardIds) {
            auto pair = findServerLists(shard);
            pair->second.vertexCollections[collection.second->getName()]
                .emplace_back(shard);
#ifdef USE_ENTERPRISE
            if (trx->isInaccessibleCollectionId(
                    collection.second->getPlanId())) {
              TRI_ASSERT(
                  ServerState::instance()->isSingleServerOrCoordinator());
              TRI_ASSERT(trxOps.skipInaccessibleCollections);
              pair->second.inaccessibleShards.insert(shard);
              pair->second.inaccessibleShards.insert(
                  std::to_string(collection.second->getCollection()->id()));
            }
#endif
          }
        }
      }
      // We have to make sure that all engines at least know all vertex
      // collections.
      // Thanks to fanout...
      for (auto const& collection : (*cs)) {
        for (auto& entry : mappingServerToCollections) {
          auto it =
              entry.second.vertexCollections.find(collection.second->getName());
          if (it == entry.second.vertexCollections.end()) {
            entry.second.vertexCollections.emplace(collection.second->getName(),
                                                   std::vector<ShardID>());
          }
        }
      }
    } else {
      // This Traversal is started with a GRAPH. It knows all relevant
      // collections.
      for (auto const& it : vertices) {
        auto shardIds = it->shardIds(restrictToShards);
        for (ShardID const& shard : *shardIds) {
          auto pair = findServerLists(shard);
          pair->second.vertexCollections[it->getName()].emplace_back(shard);
#ifdef USE_ENTERPRISE
          if (trx->isInaccessibleCollectionId(it->getPlanId())) {
            TRI_ASSERT(trxOps.skipInaccessibleCollections);
            pair->second.inaccessibleShards.insert(shard);
            pair->second.inaccessibleShards.insert(
                std::to_string(it->getCollection()->id()));
          }
#endif
        }
      }
      // We have to make sure that all engines at least know all vertex
      // collections.
      // Thanks to fanout...
      for (auto const& it : vertices) {
        for (auto& entry : mappingServerToCollections) {
          auto vIt = entry.second.vertexCollections.find(it->getName());
          if (vIt == entry.second.vertexCollections.end()) {
            entry.second.vertexCollections.emplace(it->getName(),
                                                   std::vector<ShardID>());
          }
        }
      }
    }

    // Now we have sorted all collections on the db servers. Hand the engines
    // over
    // to the server builder.

    // NOTE: This is only valid because it is single threaded and we do not
    // have concurrent access. We move out stuff from this Map => memory will
    // get corrupted if we would read it again.
    for (auto it : mappingServerToCollections) {
      // This condition is guaranteed because all shards have been prepared
      // for locking in the EngineInfos
      TRI_ASSERT(dbServerMapping.find(it.first) != dbServerMapping.end());
      dbServerMapping.find(it.first)->second.addTraverserEngine(
          en, std::move(it.second));
    }
  }
}

Result EngineInfoContainerDBServer::buildEngines(
    Query* query, std::unordered_map<std::string, std::string>& queryIds,
    std::unordered_set<std::string> const& restrictToShards,
    std::unordered_set<ShardID>& lockedShards) const {
  TRI_ASSERT(_engineStack.empty());
  LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "We have " << _engines.size()
                                          << " DBServer engines";

  // We create a map for DBServer => All Query snippets executed there
  auto dbServerMapping = createDBServerMapping(restrictToShards, lockedShards);
  // This Mapping does not contain Traversal Engines
  //
  // We add traversal engines if necessary
  injectGraphNodesToMapping(query, restrictToShards, dbServerMapping);

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  std::string const url("/_db/" + arangodb::basics::StringUtils::urlEncode(
                                      query->vocbase()->name()) +
                        "/_api/aql/setup");

  bool needCleanup = true;
  auto cleanup = [&]() {
    if (needCleanup) {
      cleanupEngines(cc, TRI_ERROR_INTERNAL, query->vocbase()->name(),
                     queryIds);
    }
  };
  TRI_DEFER(cleanup());

  std::unordered_map<std::string, std::string> headers;
  // Build Lookup Infos
  VPackBuilder infoBuilder;
  for (auto& it : dbServerMapping) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "Building Engine Info for "
                                            << it.first;
    infoBuilder.clear();
    it.second.buildMessage(query, infoBuilder);
    LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "Sending the Engine info: "
                                            << infoBuilder.toJson();

    // Now we send to DBServers.
    // We expect a body with {snippets: {id => engineId}, traverserEngines:
    // [engineId]}}

    CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto res = cc->syncRequest("", coordTransactionID, "server:" + it.first,
                               RequestType::POST, url, infoBuilder.toJson(),
                               headers, SETUP_TIMEOUT);

    if (res->getErrorCode() != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(DEBUG, Logger::AQL) << it.first << " respended with "
                                    << res->getErrorCode() << " -> "
                                    << res->stringifyErrorMessage();
      LOG_TOPIC(TRACE, Logger::AQL) << infoBuilder.toJson();
      return {res->getErrorCode(), res->stringifyErrorMessage()};
    }

    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    VPackSlice response = builder->slice();

    if (!response.isObject() || !response.get("result").isObject()) {
      // TODO could not register all engines. Need to cleanup.
      LOG_TOPIC(ERR, Logger::AQL) << "Recieved error information from "
                                  << it.first << " : " << response.toJson();
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "Unable to deploy query on all required "
              "servers. This can happen during "
              "Failover. Please check: " +
                  it.first};
    }

    VPackSlice result = response.get("result");
    VPackSlice snippets = result.get("snippets");

    for (auto const& resEntry : VPackObjectIterator(snippets)) {
      if (!resEntry.value.isString()) {
        return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                "Unable to deploy query on all required "
                "servers. This can happen during "
                "Failover. Please check: " +
                    it.first};
      }
      queryIds.emplace(resEntry.key.copyString(), resEntry.value.copyString());
    }

    VPackSlice travEngines = result.get("traverserEngines");
    if (!travEngines.isNone()) {
      if (!travEngines.isArray()) {
        // TODO could not register all traversal engines. Need to cleanup.
        return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                "Unable to deploy query on all required "
                "servers. This can happen during "
                "Failover. Please check: " +
                    it.first};
      }

      it.second.combineTraverserEngines(it.first, travEngines);
    }
  }

#ifdef USE_ENTERPRISE
  resetSatellites();
#endif
  needCleanup = false;
  return TRI_ERROR_NO_ERROR;
}

void EngineInfoContainerDBServer::addGraphNode(Query* query, GraphNode* node) {
  // Add all Edge Collections to the Transactions, Traversals do never write
  for (auto const& col : node->edgeColls()) {
    handleCollection(col.get(), AccessMode::Type::READ, false);
  }

  // Add all Vertex Collections to the Transactions, Traversals do never write
  auto& vCols = node->vertexColls();
  if (vCols.empty()) {
    // This case indicates we do not have a named graph. We simply use
    // ALL collections known to this query.
    std::map<std::string, Collection*>* cs =
        query->collections()->collections();
    for (auto const& col : *cs) {
      handleCollection(col.second, AccessMode::Type::READ, false);
    }
  } else {
    for (auto const& col : node->vertexColls()) {
      handleCollection(col.get(), AccessMode::Type::READ, false);
    }
  }

  _graphNodes.emplace_back(node);
}

/**
 * @brief Will send a shutdown to all engines registered in the list of
 * queryIds.
 * NOTE: This function will ignore all queryids where the key is not of
 * the expected format
 * they may be leftovers from Coordinator.
 * Will also clear the list of queryIds after return.
 *
 * @param cc The ClusterComm
 * @param errorCode error Code to be send to DBServers for logging.
 * @param dbname Name of the database this query is executed in.
 * @param queryIds A map of QueryIds of the format: (remoteNodeId:shardId) ->
 * queryid.
 */
void EngineInfoContainerDBServer::cleanupEngines(
    std::shared_ptr<ClusterComm> cc, int errorCode, std::string const& dbname,
    std::unordered_map<std::string, std::string>& queryIds) const {
  // Shutdown query snippets
  std::string url("/_db/" + arangodb::basics::StringUtils::urlEncode(dbname) +
                  "/_api/aql/shutdown/");
  std::vector<ClusterCommRequest> requests;
  auto body = std::make_shared<std::string>("{\"code\":" +
                                            std::to_string(errorCode) + "}");
  for (auto const& it : queryIds) {
    auto pos = it.first.find(':');
    if (pos == it.first.npos) {
      // We we get here the setup format was not as expected.
      TRI_ASSERT(false);
      continue;
    }
    auto shardId = it.first.substr(pos + 1);
    requests.emplace_back(shardId, rest::RequestType::PUT, url + it.second,
                          body);
  }

  // Shutdown traverser engines
  url = "/_db/" + arangodb::basics::StringUtils::urlEncode(dbname) +
                  "/_internal/traverser/";
  std::shared_ptr<std::string> noBody;
  for (auto const& gn : _graphNodes) {
    auto allEngines = gn->engines();
    for (auto const& engine : *allEngines) {
      requests.emplace_back(engine.first, rest::RequestType::DELETE_REQ,
                            url + basics::StringUtils::itoa(engine.second), noBody);
    }
  }

  cc->fireAndForgetRequests(requests);
  queryIds.clear();
}
