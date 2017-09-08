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

#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/IndexNode.h"
#include "Aql/ModificationNodes.h"
#include "Cluster/ClusterInfo.h"
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
    size_t id, std::vector<ExecutionNode*>&& nodes, size_t idOfRemoteNode)
    : _id(id), _nodes(nodes), _idOfRemoteNode(idOfRemoteNode) {}

EngineInfoContainerCoordinator::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes, they are managed by the AST
  // somewhere else
}

EngineInfoContainerCoordinator::EngineInfoContainerCoordinator() {}

EngineInfoContainerCoordinator::~EngineInfoContainerCoordinator() {}

QueryId EngineInfoContainerCoordinator::addQuerySnippet(
    std::vector<ExecutionNode*> nodes, size_t idOfRemoteNode) {
  // TODO: Check if the following is true:
  // idOfRemote === 0 => id === 0
  QueryId id = TRI_NewTickServer();
  _engines.emplace_back(id, std::move(nodes), idOfRemoteNode);
  return id;
}

ExecutionEngine* EngineInfoContainerCoordinator::buildEngines() {
  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                DBServer Container
// -----------------------------------------------------------------------------

EngineInfoContainerDBServer::EngineInfo::EngineInfo(
    std::vector<ExecutionNode*>&& nodes, size_t idOfRemoteNode,
    Collection const* collection)
    : _nodes(nodes),
      _idOfRemoteNode(idOfRemoteNode),
      _otherId(0),
      _collection(collection) {
  TRI_ASSERT(_collection != nullptr);
}

EngineInfoContainerDBServer::EngineInfo::~EngineInfo() {
  // This container is not responsible for nodes nor the collection,
  // they are managed by the AST somewhere else
}

void EngineInfoContainerDBServer::EngineInfo::connectQueryId(QueryId id) {
  _otherId = id;
}

EngineInfoContainerDBServer::EngineInfoContainerDBServer() {}

EngineInfoContainerDBServer::~EngineInfoContainerDBServer() {}

void EngineInfoContainerDBServer::connectLastSnippet(QueryId id) {
  if (_engines.empty()) {
    // If we do not have engines we cannot append the snippet.
    // This is the case for the initial coordinator snippet.
    return;
  }
  _engines.back().connectQueryId(id);
}

void EngineInfoContainerDBServer::addQuerySnippet(
    std::vector<ExecutionNode*> nodes, size_t idOfRemoteNode) {
  Collection const* collection = nullptr;
  auto handleCollection = [&](Collection const* col, bool isWrite) -> void {
    LOG_TOPIC(ERR, arangodb::Logger::AQL) << "Registering Collection " << col->getName();
    auto it = _collections.find(col);
    if (it == _collections.end()) {
      LOG_TOPIC(ERR, arangodb::Logger::AQL) << "As new collection";
      _collections.emplace(
          col, (isWrite ? AccessMode::Type::WRITE : AccessMode::Type::READ));
    } else {
      LOG_TOPIC(ERR, arangodb::Logger::AQL) << "and upgrade lock";
      if (isWrite && it->second == AccessMode::Type::READ) {
        // We need to upgrade the lock
        it->second = AccessMode::Type::WRITE;
      }
    }
    if (collection != nullptr && collection->isSatellite()) {
      _satellites.emplace(collection);
    }
    collection = col;
  };

  // Analyse the collections used in this Query.
  for (auto en : nodes) {
    switch (en->getType()) {
      case ExecutionNode::ENUMERATE_COLLECTION:
        handleCollection(
            static_cast<EnumerateCollectionNode*>(en)->collection(), false);
        break;
      case ExecutionNode::INDEX:
        handleCollection(static_cast<IndexNode*>(en)->collection(), false);
        break;
      case ExecutionNode::INSERT:
      case ExecutionNode::UPDATE:
      case ExecutionNode::REMOVE:
      case ExecutionNode::REPLACE:
      case ExecutionNode::UPSERT:
        handleCollection(static_cast<ModificationNode*>(en)->collection(),
                         true);
        break;
      default:
        // Do nothing
        break;
    };
  }

  _engines.emplace_back(std::move(nodes), idOfRemoteNode, collection);
}

void EngineInfoContainerDBServer::buildEngines() {
  LOG_TOPIC(ERR, arangodb::Logger::AQL) << "We have " << _engines.size() << " DBServer engines";
  std::map<ServerID, std::unordered_map<AccessMode::Type, std::vector<ShardID>>>
      serverToShardLockMapping;

  auto ci = ClusterInfo::instance();

  for (auto const& it : _collections) {
    // it.first => Collection const*
    // it.second => Lock Type
    auto shardIds = it.first->shardIds();
    for (auto const& s : *(shardIds.get())) {
      auto const servers = ci->getResponsibleServer(s);
      if (servers == nullptr || servers->empty()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE,
            "Could not find responsible server for shard " + s);
      }
      auto responsible = servers->at(0);
      auto& map = serverToShardLockMapping[responsible];
      auto& shardList = map[it.second];
      shardList.emplace_back(s);
    }
  }

  // Build Lookup Infos
  VPackBuilder infoBuilder;
  for (auto const& it: serverToShardLockMapping) {
    infoBuilder.clear();
    infoBuilder.openObject();
    infoBuilder.add(VPackValue("lockInfo"));
    infoBuilder.openObject();
    for (auto const& shardLocks : it.second) {
      switch (shardLocks.first) {
        case AccessMode::Type::READ:
          infoBuilder.add(VPackValue("READ"));
          break;
        case AccessMode::Type::WRITE:
          infoBuilder.add(VPackValue("WRITE"));
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
      infoBuilder.close(); // The array
    }
    infoBuilder.close(); // lockInfo
    // Generate the snippets
    infoBuilder.close(); // Object
    LOG_TOPIC(ERR, arangodb::Logger::FIXME) << "DBServer " << it.first << " Engine Info: " << infoBuilder.toJson();
  }
}
