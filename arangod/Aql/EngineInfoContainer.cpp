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

#include "EngineInfoContainer.h"

#include "Aql/ClusterBlocks.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             Coordinator Container
// -----------------------------------------------------------------------------

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(
    size_t id, size_t idOfRemoteNode)
    : _id(id), _idOfRemoteNode(idOfRemoteNode) {
  TRI_ASSERT(_nodes.empty());
}

EngineInfoContainerCoordinator::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes, they are managed by the AST
  // somewhere else
}

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(EngineInfo const&& other)
    : _id(other._id),
      _nodes(std::move(other._nodes)),
      _idOfRemoteNode(other._idOfRemoteNode) {
}

void EngineInfoContainerCoordinator::EngineInfo::addNode(ExecutionNode* en) {
  _nodes.emplace_back(en);
}

void EngineInfoContainerCoordinator::EngineInfo::buildEngine(
    Query* query, QueryRegistry* queryRegistry,
    std::unordered_map<std::string, std::string>& queryIds,
    std::unordered_set<ShardID> const* lockedShards) const {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(lockedShards != nullptr);
  {
    auto uniqEngine = std::make_unique<ExecutionEngine>(query);
    query->engine(uniqEngine.release());
  }

  auto engine = query->engine();

  {
    auto cpyLockedShards = std::make_unique<std::unordered_set<std::string>>(*lockedShards);
    engine->setLockedShards(cpyLockedShards.release());
  }

  auto clusterInfo = arangodb::ClusterInfo::instance();

  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
  RemoteNode* remoteNode = nullptr;

  // We need to traverse the nodes from back to front, the walker collects them in the wrong ordering
  for (auto it = _nodes.rbegin(); it != _nodes.rend(); ++it) {
    auto en = *it;
    auto const nodeType = en->getType();

    if (nodeType == ExecutionNode::REMOTE) {
      remoteNode = static_cast<RemoteNode*>(en);
      continue;
    }

    // for all node types but REMOTEs, we create blocks
    ExecutionBlock* eb =
        // ExecutionEngine::CreateBlock(engine.get(), en, cache,
        // _includedShards);
        ExecutionEngine::CreateBlock(engine, en, cache, {});

    if (eb == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal node type");
    }

    try {
      engine->addBlock(eb);
    } catch (...) {
      delete eb;
      throw;
    }

    for (auto const& dep : en->getDependencies()) {
      auto d = cache.find(dep);

      if (d != cache.end()) {
        // add regular dependencies
        TRI_ASSERT((*d).second != nullptr);
        eb->addDependency((*d).second);
      }
    }

    if (nodeType == ExecutionNode::GATHER) {
      // we found a gather node
      if (remoteNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "expecting a remoteNode");
      }

      // now we'll create a remote node for each shard and add it to the
      // gather node
      auto gatherNode = static_cast<GatherNode const*>(en);
      Collection const* collection = gatherNode->collection();

      // auto shardIds = collection->shardIds(_includedShards);
      auto shardIds = collection->shardIds();
      for (auto const& shardId : *shardIds) {
        std::string theId =
            arangodb::basics::StringUtils::itoa(remoteNode->id()) + ":" +
            shardId;

        auto it = queryIds.find(theId);
        if (it == queryIds.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                         "could not find query id in list");
        }

        auto serverList = clusterInfo->getResponsibleServer(shardId);
        if (serverList->empty()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
              "Could not find responsible server for shard " + shardId);
        }

        // use "server:" instead of "shard:" to send query fragments to
        // the correct servers, even after failover or when a follower drops
        // the problem with using the previous shard-based approach was that
        // responsibilities for shards may change at runtime.
        // however, an AQL query must send all requests for the query to the
        // initially used servers.
        // if there is a failover while the query is executing, we must still
        // send all following requests to the same servers, and not the newly
        // responsible servers.
        // otherwise we potentially would try to get data from a query from
        // server B while the query was only instanciated on server A.
        TRI_ASSERT(!serverList->empty());
        auto& leader = (*serverList)[0];
        ExecutionBlock* r = new RemoteBlock(engine, remoteNode,
                                            "server:" + leader,  // server
                                            "",                  // ownName
                                            it->second);         // queryId

        try {
          engine->addBlock(r);
        } catch (...) {
          delete r;
          throw;
        }

        TRI_ASSERT(r != nullptr);
        eb->addDependency(r);
      }
    }

    // the last block is always the root
    engine->root(eb);

    // put it into our cache:
    cache.emplace(en, eb);
  }

  TRI_ASSERT(engine->root() != nullptr);

  LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "Storing Coordinator engine: "
                                        << _id;

  // For _id == 0 this thread will always maintain the handle to
  // the engine and will clean up. We do not keep track of it seperately
  if (_id != 0) {
    try {
      queryRegistry->insert(_id, query, 600.0);
    } catch (...) {
      // TODO Is this correct or does it cause failures?
      // TODO Add failure tests
      delete engine->getQuery();
      // This deletes the new query as well as the engine
      throw;
    }

    try {
      std::string queryId = arangodb::basics::StringUtils::itoa(_id);
      std::string theID = arangodb::basics::StringUtils::itoa(_idOfRemoteNode) +
                          "/" + engine->getQuery()->vocbase()->name();
      queryIds.emplace(theID, queryId);
    } catch (...) {
      queryRegistry->destroy(engine->getQuery()->vocbase(), _id,
                             TRI_ERROR_INTERNAL);
      // This deletes query, engine and entry in QueryRegistry
      throw;
    }
  }
}

EngineInfoContainerCoordinator::EngineInfoContainerCoordinator() {
  // We always start with an empty coordinator snippet
  _engines.emplace_back(0, 0);
  _engineStack.emplace(0);
}

EngineInfoContainerCoordinator::~EngineInfoContainerCoordinator() {}

void EngineInfoContainerCoordinator::addNode(ExecutionNode* node) {
#ifdef USE_MAINTAINER_MODE
  switch (node->getType()) {
    case ExecutionNode::INDEX:
    case ExecutionNode::ENUMERATE_COLLECTION:
      // These node types cannot be executed on coordinator side
      TRI_ASSERT(false);
    default:
      break;
  }
#endif
  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());
  size_t idx = _engineStack.top();
  _engines[idx].addNode(node);
}

void EngineInfoContainerCoordinator::openSnippet(size_t idOfRemoteNode) {
  _engineStack.emplace(_engines.size()); // Insert next id
  QueryId id = TRI_NewTickServer();
  _engines.emplace_back(id, idOfRemoteNode);
}

QueryId EngineInfoContainerCoordinator::closeSnippet() {
  TRI_ASSERT(!_engines.empty());
  TRI_ASSERT(!_engineStack.empty());

  size_t idx = _engineStack.top();
  QueryId id = _engines[idx].queryId();
  _engineStack.pop();
  return id;
}

ExecutionEngine* EngineInfoContainerCoordinator::buildEngines(
    Query* query, QueryRegistry* registry,
    std::unordered_map<std::string, std::string>& queryIds,
    std::unordered_set<ShardID> const* lockedShards) const {
  TRI_ASSERT(_engineStack.size() == 1);
  TRI_ASSERT(_engineStack.top() == 0);

  bool first = true;
  Query* localQuery = query;
  for (auto const& info : _engines) {
    if (!first) {
      // need a new query instance on the coordinator
      localQuery = query->clone(PART_DEPENDENT, false);
      if (localQuery == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "cannot clone query");
      }
    }
    try {
      info.buildEngine(localQuery, registry, queryIds, lockedShards);
    } catch (...) {
      localQuery->engine(nullptr);  // engine is already destroyed internally
      if (!first) {
        delete localQuery;
      }
      throw;
    }

    first = false;
  }

  // Why return?
  return query->engine();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                DBServer Container
// -----------------------------------------------------------------------------

EngineInfoContainerDBServer::EngineInfo::EngineInfo(
    size_t idOfRemoteNode)
    : _idOfRemoteNode(idOfRemoteNode),
      _otherId(0),
      _collection(nullptr) { }

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
    Query* query, ShardID id, VPackBuilder& infoBuilder, bool isResponsibleForInit) const {
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
  {
    VPackBuilder hund;
    plan.root()->toVelocyPack(hund, true);
  }
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
          static_cast<EnumerateCollectionNode*>(node)->collection(), false);
      break;
    case ExecutionNode::INDEX:
      handleCollection(
          static_cast<EnumerateCollectionNode*>(node)->collection(), false);
      break;
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT:
      handleCollection(static_cast<ModificationNode*>(node)->collection(),
                       true);
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
  _engines[e->collection()].emplace_back(std::move(e));
}

// This first defines the lock required for this collection
// Then we update the collection pointer of the last engine.

void EngineInfoContainerDBServer::handleCollection(Collection const* col,
    bool isWrite) {
  auto it = _collections.find(col);
  if (it == _collections.end()) {
    _collections.emplace(
        col, (isWrite ? AccessMode::Type::WRITE : AccessMode::Type::READ));
  } else {
    if (isWrite && it->second == AccessMode::Type::READ) {
      // We need to upgrade the lock
      it->second = AccessMode::Type::WRITE;
    }
  }
  TRI_ASSERT(!_engineStack.empty());
  auto e = _engineStack.top();
  auto formerCol = e->collection();
  if (formerCol != nullptr && formerCol->isSatellite()) {
    _satellites.emplace(formerCol);
  }
  // ... const_cast
  e->collection(const_cast<Collection*>(col));
}

EngineInfoContainerDBServer::DBServerInfo::DBServerInfo() {}
EngineInfoContainerDBServer::DBServerInfo::~DBServerInfo() {}

void EngineInfoContainerDBServer::DBServerInfo::addShardLock(
    AccessMode::Type const& lock, ShardID const& id) {
  _shardLocking[lock].emplace_back(id);
}

void EngineInfoContainerDBServer::DBServerInfo::addEngine(
    std::shared_ptr<EngineInfoContainerDBServer::EngineInfo> info, ShardID const& id) {
  _engineInfos[info].emplace_back(id);
}

void EngineInfoContainerDBServer::DBServerInfo::buildMessage(
    Query* query, VPackBuilder& infoBuilder) const {
  TRI_ASSERT(infoBuilder.isEmpty());

  infoBuilder.openObject();
  infoBuilder.add(VPackValue("lockInfo"));
  infoBuilder.openObject();
  for (auto const& shardLocks : _shardLocking) {
    switch (shardLocks.first) {
      case AccessMode::Type::READ:
      case AccessMode::Type::WRITE:
        infoBuilder.add(VPackValue(AccessMode::typeString(shardLocks.first)));
        break;
      default:
        // We only have Read and Write Locks in Cluster.
        // NONE or EXCLUSIVE is impossible
        TRI_ASSERT(false);
        continue;
    }

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
  infoBuilder.close();  // Object
}

void EngineInfoContainerDBServer::DBServerInfo::injectQueryOptions(
    Query* query, VPackBuilder& infoBuilder) const {
  // the toVelocyPack will open & close the "options" object
  query->queryOptions().toVelocyPack(infoBuilder, true);
}

void EngineInfoContainerDBServer::buildEngines(
    Query* query,
    std::unordered_map<std::string, std::string>& queryIds,
    std::unordered_set<ShardID>* lockedShards) const {
  TRI_ASSERT(_engineStack.empty());
  LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "We have " << _engines.size()
                                          << " DBServer engines";
  std::map<ServerID, DBServerInfo> dbServerMapping;

  auto ci = ClusterInfo::instance();

  for (auto const& it : _collections) {
    // it.first => Collection const*
    // it.second => Lock Type
    std::vector<std::shared_ptr<EngineInfo>> const* engines = nullptr;
    if (_engines.find(it.first) != _engines.end()) {
      engines = &_engines.find(it.first)->second;
    }
    auto shardIds = it.first->shardIds();
    for (auto const& s : *(shardIds.get())) {
      lockedShards->emplace(s);
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

  auto cc = ClusterComm::instance();

  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return;
  }

  std::string const url("/_db/" + arangodb::basics::StringUtils::urlEncode(
                                      query->vocbase()->name()) +
                        "/_api/aql/setup");

  std::unordered_map<std::string, std::string> headers;
  // Build Lookup Infos
  VPackBuilder infoBuilder;
  for (auto const& it : dbServerMapping) {
    LOG_TOPIC(DEBUG, arangodb::Logger::AQL) << "Building Engine Info for "
                                            << it.first;
    infoBuilder.clear();
    it.second.buildMessage(query, infoBuilder);

    // Now we send to DBServers. We expect a body with {id => engineId} plus 0
    // => trxEngine
    CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto res = cc->syncRequest("", coordTransactionID, "server:" + it.first,
                               RequestType::POST, url, infoBuilder.toJson(),
                               headers, 90.0);

    if (res->getErrorCode() != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC(ERR, Logger::AQL) << infoBuilder.toJson();
      // TODO could not register all engines. Need to cleanup.
      THROW_ARANGO_EXCEPTION_MESSAGE(res->getErrorCode(),
                                     res->stringifyErrorMessage());
    }

    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    VPackSlice response = builder->slice();

    if (!response.isObject() || !response.get("result").isObject()) {
      // TODO could not register all engines. Need to cleanup.
      LOG_TOPIC(ERR, Logger::AQL) << "Recieved error information from " << it.first << " : " << response.toJson();
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                                     "Unable to deploy query on all required "
                                     "servers. This can happen during "
                                     "Failover. Please check: " +
                                         it.first);
    }

    for (auto const& resEntry : VPackObjectIterator(response.get("result"))) {
      if (!resEntry.value.isString()) {
        // TODO could not register all engines. Need to cleanup.
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                                       "Unable to deploy query on all required "
                                       "servers. This can happen during "
                                       "Failover. Please check: " +
                                           it.first);
      }
      queryIds.emplace(resEntry.key.copyString(), resEntry.value.copyString());
    }
  }
}

void EngineInfoContainerDBServer::addGraphNode(GraphNode* node) {
  // TODO Parse Collections
  _graphNodes.emplace(node);
}
