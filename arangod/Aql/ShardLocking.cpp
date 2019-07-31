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

#include "ShardLocking.h"

#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Query.h"
#include "Utils/CollectionNameResolver.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

std::set<ShardID> const ShardLocking::EmptyShardList{};

void ShardLocking::CollectionLockingInformation::mergeShards(
    std::shared_ptr<std::vector<ShardID>> const& shards) {
  for (auto const& s : *shards) {
    usedShards.emplace(s);
  }
}

void ShardLocking::addNode(ExecutionNode const* baseNode) {
  TRI_ASSERT(baseNode != nullptr);
  // If we have ever accessed the server lists,
  // we cannot insert Nodes anymore.
  // If this needs to be modified in the future, this could
  // should clear the below lists, fiddling out the diff
  // is rather confusing.
  TRI_ASSERT(_serverToLockTypeToShard.empty());
  TRI_ASSERT(_serverToCollectionToShard.empty());
  switch (baseNode->getType()) {
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::TRAVERSAL: {
      // Add GraphNode
      auto node = ExecutionNode::castTo<GraphNode const*>(baseNode);
      if (node == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to GraphNode");
      }
      // Add all Edge Collections to the Transactions, Traversals do never write
      for (auto const& col : node->edgeColls()) {
        updateLocking(col.get(), AccessMode::Type::READ, {});
      }

      // Add all Vertex Collections to the Transactions, Traversals do never write
      auto& vCols = node->vertexColls();
      if (vCols.empty()) {
        TRI_ASSERT(_query);
        auto& resolver = _query->resolver();

        // This case indicates we do not have a named graph. We simply use
        // ALL collections known to this query.
        std::map<std::string, Collection*> const* cs =
            _query->collections()->collections();
        for (auto const& col : *cs) {
          if (!resolver.getCollection(col.first)) {
            // not a collection, filter out
            continue;
          }
          updateLocking(col.second, AccessMode::Type::READ, {});
        }
      } else {
        for (auto const& col : node->vertexColls()) {
          updateLocking(col.get(), AccessMode::Type::READ, {});
        }
      }
      break;
    }
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX: {
      auto const* colNode = ExecutionNode::castTo<CollectionAccessingNode const*>(baseNode);
      if (colNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "unable to cast node to CollectionAccessingNode");
      }
      std::unordered_set<std::string> restrictedShard;
      if (colNode->isRestricted()) {
        restrictedShard.emplace(colNode->restrictedShard());
      }

      auto const* col = colNode->collection();
      updateLocking(col, AccessMode::Type::READ, restrictedShard);
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode const*>(baseNode);
      if (viewNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to ViewNode");
      }
      for (aql::Collection const& col : viewNode->collections()) {
        updateLocking(&col, AccessMode::Type::READ, {});
      }

      break;
    }
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT: {
      auto const* modNode = ExecutionNode::castTo<ModificationNode const*>(baseNode);
      if (modNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "unable to cast node to ModificationNode");
      }
      auto const* col = modNode->collection();

      std::unordered_set<std::string> restrictedShard;
      if (modNode->isRestricted()) {
        restrictedShard.emplace(modNode->restrictedShard());
      }

      updateLocking(col,
                    modNode->getOptions().exclusive ? AccessMode::Type::EXCLUSIVE
                                                    : AccessMode::Type::WRITE,
                    restrictedShard);
      break;
    }
    default:
      // Nothing todo
      break;
  }
}

void ShardLocking::updateLocking(Collection const* col, AccessMode::Type const& accessType,
                                 std::unordered_set<std::string> const& restrictedShards) {
  auto const shards = col->shardIds(
      restrictedShards.empty() ? _query->queryOptions().shardIds : restrictedShards);

  // What if we have an empty shard list here?
  if (shards->empty()) {
    LOG_TOPIC("0997e", WARN, arangodb::Logger::AQL)
        << "TEMPORARY: A collection access of a query has no result in any "
           "shard";
  }

  auto& info = _collectionLocking[col];
  // We need to upgrade the lock
  info.lockType = (std::max)(info.lockType, accessType);
  info.mergeShards(shards);
}

std::vector<Collection const*> ShardLocking::getUsedCollections() const {
  std::vector<Collection const*> result;
  std::transform(_collectionLocking.begin(), _collectionLocking.end(),
                 std::back_inserter(result),
                 [](auto const& item) { return item.first; });
  return result;
}

std::vector<ServerID> ShardLocking::getRelevantServers() {
  if (_serverToCollectionToShard.empty()) {
    TRI_ASSERT(_serverToLockTypeToShard.empty());
    // Will internally fetch shards if not existing
    auto shardMapping = getShardMapping();
    // Now we need to create the mappings
    // server => locktype => [shards]
    // server => collection => [shards](sorted)
    for (auto const& lockInfo : _collectionLocking) {
      for (auto const& sid : lockInfo.second.usedShards) {
        auto server = shardMapping.find(sid);
        // If shard has no leader the above call should have thrown!
        TRI_ASSERT(server != shardMapping.end());
        // We will create all maps as empty default constructions on the way
        _serverToCollectionToShard[server->second][lockInfo.first].emplace(sid);
        _serverToLockTypeToShard[server->second][lockInfo.second.lockType].emplace(sid);
      }
    }
  }
  std::vector<ServerID> result;
  std::transform(_serverToCollectionToShard.begin(),
                 _serverToCollectionToShard.end(), std::back_inserter(result),
                 [](auto const& item) { return item.first; });
  return result;
}

void ShardLocking::serializeIntoBuilder(ServerID const& server,
                                        arangodb::velocypack::Builder& builder) const {
  // We NEED to have some lock infomration for every server, wo do not allow
  // servers that are basically not responsible for data here.
  auto lockInfo = _serverToLockTypeToShard.find(server);
  TRI_ASSERT(lockInfo != _serverToLockTypeToShard.end());
  for (auto const& it : lockInfo->second) {
    TRI_ASSERT(builder.isOpenObject());
    AccessMode::Type type;
    std::unordered_set<ShardID> shards;
    std::tie(type, shards) = it;
    builder.add(VPackValue(AccessMode::typeString(type)));
    builder.openArray();
    for (auto const& s : shards) {
      builder.add(VPackValue(s));
    }
    builder.close();  // Array
  }
  TRI_ASSERT(builder.isOpenObject());
}

std::unordered_map<ShardID, ServerID> const& ShardLocking::getShardMapping() {
  if (_shardMapping.empty()) {
    std::unordered_set<ShardID> shardIds;
    for (auto const& lockInfo : _collectionLocking) {
      for (auto const& sid : lockInfo.second.usedShards) {
        shardIds.emplace(sid);
      }
    }
    auto ci = ClusterInfo::instance();
    if (ci == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
    _shardMapping = ci->getResponsibleServers(shardIds);
  }
  return _shardMapping;
}