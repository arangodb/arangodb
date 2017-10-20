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

#include "EngineInfoContainerCoordinator.h"

#include "Aql/ClusterBlocks.h"
#include "Aql/ClusterNodes.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "VocBase/ticks.h"

using namespace arangodb;
using namespace arangodb::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                             Coordinator Container
// -----------------------------------------------------------------------------

EngineInfoContainerCoordinator::EngineInfo::EngineInfo(size_t id,
                                                       size_t idOfRemoteNode)
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
      _idOfRemoteNode(other._idOfRemoteNode) {}

void EngineInfoContainerCoordinator::EngineInfo::addNode(ExecutionNode* en) {
  _nodes.emplace_back(en);
}

void EngineInfoContainerCoordinator::EngineInfo::buildEngine(
    Query* query, QueryRegistry* queryRegistry,
    std::unordered_set<std::string> const& restrictToShards,
    std::unordered_map<std::string, std::string>& queryIds,
    std::unordered_set<ShardID> const* lockedShards) const {
  TRI_ASSERT(!_nodes.empty());
  TRI_ASSERT(lockedShards != nullptr);
  {
    auto uniqEngine = std::make_unique<ExecutionEngine>(query);
    query->setEngine(uniqEngine.release());
  }

  auto engine = query->engine();

  {
    auto cpyLockedShards =
        std::make_unique<std::unordered_set<std::string>>(*lockedShards);
    engine->setLockedShards(cpyLockedShards.release());
  }

  auto clusterInfo = arangodb::ClusterInfo::instance();

  std::unordered_map<ExecutionNode*, ExecutionBlock*> cache;
  RemoteNode* remoteNode = nullptr;

  // We need to traverse the nodes from back to front, the walker collects them
  // in the wrong ordering
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

      auto shardIds = collection->shardIds(restrictToShards);
      for (auto const& shardId : *shardIds) {
        std::string theId =
            arangodb::basics::StringUtils::itoa(remoteNode->id()) + ":" +
            shardId;

        auto it = queryIds.find(theId);
        if (it == queryIds.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_INTERNAL,
              "could not find query id " + theId + " in list");
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
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
  _engineStack.emplace(_engines.size());  // Insert next id
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
    std::unordered_set<std::string> const& restrictToShards,
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
      info.buildEngine(localQuery, registry, restrictToShards, queryIds,
                       lockedShards);
    } catch (...) {
      localQuery->releaseEngine();
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
