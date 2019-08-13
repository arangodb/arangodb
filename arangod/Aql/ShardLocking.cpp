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

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

std::set<ShardID> const ShardLocking::EmptyShardList{};
std::unordered_set<ShardID> const ShardLocking::EmptyShardListUnordered{};

void ShardLocking::addNode(ExecutionNode const* baseNode, size_t snippetId) {
  TRI_ASSERT(baseNode != nullptr);
  // If we have ever accessed the server lists,
  // we cannot insert Nodes anymore.
  // If this needs to be modified in the future, this could
  // should clear the below lists, fiddling out the diff
  // is rather confusing.
  TRI_ASSERT(_serverToLockTypeToShard.empty());
  TRI_ASSERT(_serverToCollectionToShard.empty());
  switch (baseNode->getType()) {
    case ExecutionNode::K_SHORTEST_PATHS:
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
        updateLocking(col.get(), AccessMode::Type::READ, snippetId, {});
      }

      // Add all Vertex Collections to the Transactions, Traversals do never
      // write, the collections have been adjusted already
      for (auto const& col : node->vertexColls()) {
        updateLocking(col.get(), AccessMode::Type::READ, snippetId, {});
      }
      break;
    }
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX: {
      auto const* colNode = dynamic_cast<CollectionAccessingNode const*>(baseNode);
      if (colNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "unable to cast node to CollectionAccessingNode");
      }
      std::unordered_set<std::string> restrictedShard;
      if (colNode->isRestricted()) {
        restrictedShard.emplace(colNode->restrictedShard());
      }

      auto* col = colNode->collection();
      updateLocking(col, AccessMode::Type::READ, snippetId, restrictedShard);
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode = ExecutionNode::castTo<iresearch::IResearchViewNode const*>(baseNode);
      if (viewNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to ViewNode");
      }
      for (aql::Collection const& col : viewNode->collections()) {
        updateLocking(&col, AccessMode::Type::READ, snippetId, {});
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
      auto* col = modNode->collection();

      std::unordered_set<std::string> restrictedShard;
      if (modNode->isRestricted()) {
        restrictedShard.emplace(modNode->restrictedShard());
      }

      updateLocking(col,
                    modNode->getOptions().exclusive ? AccessMode::Type::EXCLUSIVE
                                                    : AccessMode::Type::WRITE,
                    snippetId, restrictedShard);
      break;
    }
    default:
      // Nothing todo
      break;
  }
}

void ShardLocking::updateLocking(Collection const* col,
                                 AccessMode::Type const& accessType, size_t snippetId,
                                 std::unordered_set<std::string> const& restrictedShards) {
  auto& info = _collectionLocking[col];
  // We need to upgrade the lock
  info.lockType = (std::max)(info.lockType, accessType);
  if (info.allShards.empty()) {
    // Load shards only once per collection!
    auto const shards = col->shardIds(_query->queryOptions().shardIds);
    // What if we have an empty shard list here?
    if (shards->empty()) {
      LOG_TOPIC("0997e", WARN, arangodb::Logger::AQL)
          << "TEMPORARY: A collection access of a query has no result in any "
             "shard";
      TRI_ASSERT(false);
      return;
    }
    for (auto const& s : *shards) {
      info.allShards.emplace(s);
    }
  }
  auto snippetPart = info.restrictedSnippets.find(snippetId);
  if (snippetPart == info.restrictedSnippets.end()) {
    std::tie(snippetPart, std::ignore) = info.restrictedSnippets.emplace(
        snippetId, std::make_pair(false, std::unordered_set<ShardID>{}));
  }
  TRI_ASSERT(snippetPart != info.restrictedSnippets.end());

  if (!restrictedShards.empty()) {
    // Restricted case, store the restriction for the snippet.
    bool& isRestricted = snippetPart->second.first;
    std::unordered_set<ShardID>& shards = snippetPart->second.second;
    if (isRestricted) {
      // We are already restricted, only possible if the restriction is identical
      TRI_ASSERT(shards == restrictedShards);
      if (shards != restrictedShards) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "Restricted a snippet to two distinct shards of a collection.");
      }
    } else {
      isRestricted = true;
      for (auto const& s : restrictedShards) {
        if (info.allShards.find(s) == info.allShards.end()) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
              "Restricting: " + col->name() + " to shard " + s +
                  " which it does not have, or is excluded in the query");
        }
        shards.emplace(s);
      }
    }
  }
}

std::vector<Collection const*> ShardLocking::getUsedCollections() const {
  std::vector<Collection const*> result;
  std::transform(_collectionLocking.begin(), _collectionLocking.end(),
                 std::back_inserter(result),
                 [](auto const& item) { return item.first; });
  return result;
}

std::vector<ServerID> ShardLocking::getRelevantServers() {
  if (_collectionLocking.empty()) {
    // Nothing todo, there are no DBServers
    return {};
  }
  if (_serverToCollectionToShard.empty()) {
    TRI_ASSERT(_serverToLockTypeToShard.empty());
    // Will internally fetch shards if not existing
    auto shardMapping = getShardMapping();
    // Now we need to create the mappings
    // server => locktype => [shards]
    // server => collection => [shards](sorted)
    for (auto& lockInfo : _collectionLocking) {
      std::unordered_set<ShardID> shardsToLock;
      for (auto const& sid : lockInfo.second.allShards) {
        auto server = shardMapping.find(sid);
        if (server != shardMapping.end()) {
          // We will create all maps as empty default constructions on the way
          _serverToCollectionToShard[server->second][lockInfo.first].emplace(sid);
          _serverToLockTypeToShard[server->second][lockInfo.second.lockType].emplace(sid);
        }
      }
    }
  }
  std::vector<ServerID> result;
  std::transform(_serverToCollectionToShard.begin(), _serverToCollectionToShard.end(),
                 std::back_inserter(result), [](auto const& item) {
                   TRI_ASSERT(!item.first.empty());
                   return item.first;
                 });
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
  if (_shardMapping.empty() && !_collectionLocking.empty()) {
    std::unordered_set<ShardID> shardIds;
    for (auto& lockInfo : _collectionLocking) {
      auto& allShards = lockInfo.second.allShards;
      TRI_ASSERT(!allShards.empty());
      for (auto const& rest : lockInfo.second.restrictedSnippets) {
        if (!rest.second.first) {
          // We have an unrestricted snippet for this collection
          // Use all shards for locking
          for (auto const& s : allShards) {
            shardIds.emplace(s);
          }
          // No need to search further
          break;
        }
        for (auto const& s : rest.second.second) {
          shardIds.emplace(s);
        }
      }
    }
    auto ci = ClusterInfo::instance();
    if (ci == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
    // We have at least one shard, otherwise we would not have snippets!
    TRI_ASSERT(!shardIds.empty());
    _shardMapping = ci->getResponsibleServers(shardIds);
    TRI_ASSERT(_shardMapping.size() == shardIds.size());
    for (auto const& lockInfo : _collectionLocking) {
      for (auto const& sid : lockInfo.second.allShards) {
        auto mapped = _shardMapping.find(sid);
        if (mapped != _shardMapping.end()) {
          lockInfo.first->addShardToServer(sid, mapped->second);
        }
      }
    }
  }
  return _shardMapping;
}

std::unordered_set<ShardID> const& ShardLocking::shardsForSnippet(size_t snippetId,
                                                                  Collection const* col) {
  auto const& lockInfo = _collectionLocking.find(col);

  TRI_ASSERT(lockInfo != _collectionLocking.end());
  if (lockInfo == _collectionLocking.end()) {
    // We ask for a collection we did not lock!
    return EmptyShardListUnordered;
  }
  auto restricted = lockInfo->second.restrictedSnippets.find(snippetId);
  // We are asking for shards of a collection that are not registered with this
  TRI_ASSERT(restricted != lockInfo->second.restrictedSnippets.end());
  if (restricted == lockInfo->second.restrictedSnippets.end()) {
    return EmptyShardListUnordered;
  }
  if (restricted->second.first /* isRestricted */) {
    return restricted->second.second;
  }
  return lockInfo->second.allShards;
}