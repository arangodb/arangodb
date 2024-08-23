////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode/CollectionAccessingNode.h"
#include "Aql/ExecutionNode/ExecutionNode.h"
#include "Aql/ExecutionNode/GraphNode.h"
#include "Aql/ExecutionNode/IResearchViewNode.h"
#include "Aql/ExecutionNode/JoinNode.h"
#include "Aql/ExecutionNode/ModificationNode.h"
#include "Aql/OptimizerRule.h"
#include "Aql/Query.h"
#include "Cluster/ClusterFeature.h"
#include "Logger/LogMacros.h"
#include "Metrics/Counter.h"
#include "StorageEngine/TransactionState.h"
#include "Utilities/NameValidator.h"

#include <absl/strings/str_cat.h>

#include <algorithm>

using namespace arangodb;
using namespace arangodb::aql;

std::set<ShardID> const ShardLocking::EmptyShardList{};
std::unordered_set<ShardID> const ShardLocking::EmptyShardListUnordered{};

void ShardLocking::addNode(ExecutionNode const* baseNode, size_t snippetId,
                           bool pushToSingleServer) {
  TRI_ASSERT(baseNode != nullptr);

  std::string const& forceOneShardAttributeValue =
      _query.queryOptions().forceOneShardAttributeValue;
  bool useRestrictedShard =
      pushToSingleServer && !forceOneShardAttributeValue.empty();

  auto addRestrictedShard = [&](aql::Collection const* col,
                                std::unordered_set<ShardID>& restrictedShards) {
    TRI_ASSERT(!forceOneShardAttributeValue.empty());
    TRI_ASSERT(useRestrictedShard);
    auto maybeShardID = std::invoke([&]() {
      if (col->isDisjoint()) {
        // if disjoint smart edge collection, we must insert an
        // artifical key with two colons, to pretend it is a real
        // smart graph key
        return col->getCollection()->getResponsibleShard(
            absl::StrCat(forceOneShardAttributeValue,
                         ":test:", forceOneShardAttributeValue));
      } else {
        auto const& shardKeys = col->getCollection().get()->shardKeys();
        TRI_ASSERT(!shardKeys.empty());
        auto const& shardKey = shardKeys.at(0);
        if (shardKey == StaticStrings::PrefixOfKeyString) {
          return col->getCollection()->getResponsibleShard(
              absl::StrCat(forceOneShardAttributeValue, ":test"));
        } else if (shardKey == StaticStrings::PostfixOfKeyString) {
          return col->getCollection()->getResponsibleShard(
              absl::StrCat("test:", forceOneShardAttributeValue));
        } else {
          VPackBuilder builder;
          {
            VPackObjectBuilder guard(&builder);
            builder.add(shardKey, VPackValue(forceOneShardAttributeValue));
          }
          return col->getCollection()->getResponsibleShard(builder.slice(),
                                                           false);
        }
      }
    });
    if (maybeShardID.fail()) {
      THROW_ARANGO_EXCEPTION(maybeShardID.result());
    }

    restrictedShards.emplace(std::move(maybeShardID.get()));
  };

  // If we have ever accessed the server lists,
  // we cannot insert Nodes anymore.
  // If this needs to be modified in the future, this could
  // should clear the below lists, fiddling out the diff
  // is rather confusing.
  TRI_ASSERT(_serverToLockTypeToShard.empty());
  TRI_ASSERT(_serverToCollectionToShard.empty());
  switch (baseNode->getType()) {
    case ExecutionNode::ENUMERATE_PATHS:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::TRAVERSAL: {
      // Add GraphNode
      auto* graphNode = ExecutionNode::castTo<GraphNode const*>(baseNode);
      if (graphNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to GraphNode");
      }
      auto const graphIsUsedAsSatellite = graphNode->isUsedAsSatellite();
      auto const isUsedAsSatellite = [&](auto const& col) {
        return graphIsUsedAsSatellite ||
               (col->isSatellite() &&
                (pushToSingleServer || graphNode->isSmart()));
      };
      // Add all Edge Collections to the Transactions, Traversals do never write
      for (auto const& col : graphNode->edgeColls()) {
        std::unordered_set<ShardID> restrictedShards;
        if (useRestrictedShard) {
          addRestrictedShard(col, restrictedShards);
        }
        updateLocking(col, AccessMode::Type::READ, snippetId, restrictedShards,
                      isUsedAsSatellite(col));
      }

      // Add all Vertex Collections to the Transactions, Traversals do never
      // write, the collections have been adjusted already
      for (auto const& col : graphNode->vertexColls()) {
        std::unordered_set<ShardID> restrictedShards;
        if (useRestrictedShard) {
          addRestrictedShard(col, restrictedShards);
        }
        updateLocking(col, AccessMode::Type::READ, snippetId, restrictedShards,
                      isUsedAsSatellite(col));
      }

      break;
    }
    case ExecutionNode::ENUMERATE_COLLECTION:
    case ExecutionNode::INDEX: {
      auto const* colNode =
          dynamic_cast<CollectionAccessingNode const*>(baseNode);
      if (colNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "unable to cast node to CollectionAccessingNode");
      }
      std::unordered_set<ShardID> restrictedShards;
      if (useRestrictedShard) {
        addRestrictedShard(colNode->collection(), restrictedShards);
      } else if (colNode->isRestricted()) {
        restrictedShards.emplace(colNode->restrictedShard());
      }

      auto* col = colNode->collection();
      updateLocking(col, AccessMode::Type::READ, snippetId, restrictedShards,
                    colNode->isUsedAsSatellite());
      break;
    }
    case ExecutionNode::JOIN: {
      auto joinNode = ExecutionNode::castTo<JoinNode const*>(baseNode);
      if (joinNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to JoinNode");
      }
      for (auto const& idx : joinNode->getIndexInfos()) {
        std::unordered_set<ShardID> restrictedShards;
        TRI_ASSERT(!useRestrictedShard);

        auto* col = idx.collection;
        updateLocking(col, AccessMode::Type::READ, snippetId, restrictedShards,
                      idx.usedAsSatellite);
      }
      break;
    }
    case ExecutionNode::ENUMERATE_IRESEARCH_VIEW: {
      auto viewNode =
          ExecutionNode::castTo<iresearch::IResearchViewNode const*>(baseNode);
      if (viewNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "unable to cast node to ViewNode");
      }
      // TODO Enumerate for indexes, not collection?
      for (auto const& [collection, _] : viewNode->collections()) {
        std::unordered_set<ShardID> restrictedShards;
        if (useRestrictedShard) {
          addRestrictedShard(&(collection.get()), restrictedShards);
        }
        updateLocking(&(collection.get()), AccessMode::Type::READ, snippetId,
                      restrictedShards, false);
      }
      break;
    }
    case ExecutionNode::INSERT:
    case ExecutionNode::UPDATE:
    case ExecutionNode::REMOVE:
    case ExecutionNode::REPLACE:
    case ExecutionNode::UPSERT: {
      auto const* modNode =
          ExecutionNode::castTo<ModificationNode const*>(baseNode);
      if (modNode == nullptr) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL, "unable to cast node to ModificationNode");
      }
      auto* col = modNode->collection();

      std::unordered_set<ShardID> restrictedShards;
      if (useRestrictedShard) {
        addRestrictedShard(modNode->collection(), restrictedShards);
      } else if (modNode->isRestricted()) {
        restrictedShards.emplace(modNode->restrictedShard());
      }
      // Not supported yet
      TRI_ASSERT(!modNode->isUsedAsSatellite());
      updateLocking(col,
                    modNode->getOptions().exclusive
                        ? AccessMode::Type::EXCLUSIVE
                        : AccessMode::Type::WRITE,
                    snippetId, restrictedShards, modNode->isUsedAsSatellite());
      break;
    }
    default:
      // Nothing todo
      break;
  }
}

void ShardLocking::updateLocking(
    Collection const* col, AccessMode::Type const& accessType, size_t snippetId,
    std::unordered_set<ShardID> const& restrictedShards, bool usedAsSatellite) {
  auto& info = _collectionLocking[col];
  // We need to upgrade the lock
  info.lockType = (std::max)(info.lockType, accessType);
  if (usedAsSatellite) {
    info.isSatellite = true;
  }
  if (info.allShards.empty()) {
    // Load shards only once per collection!
    auto const shards = col->shardIds(_query.queryOptions().restrictToShards);
    // What if we have an empty shard list here?
    if (shards->empty()) {
      auto const& name = col->name();
      if (!NameValidator::isSystemName(name)) {
        LOG_TOPIC("0997e", WARN, arangodb::Logger::AQL)
            << "Accessing collection: " << name
            << " does not translate to any shard. Aborting query.";
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
          absl::StrCat("Could not identify any shard belonging to collection: ",
                       name, ". Maybe it is dropped?"));
    }
    for (auto const& s : *shards) {
      info.allShards.emplace(s);
    }
  }
  auto snippetPart = info.snippetInfo.find(snippetId);
  if (snippetPart == info.snippetInfo.end()) {
    std::tie(snippetPart, std::ignore) =
        info.snippetInfo.try_emplace(snippetId, SnippetInformation{});
  }
  TRI_ASSERT(snippetPart != info.snippetInfo.end());
  // cppcheck-suppress derefInvalidIteratorRedundantCheck
  SnippetInformation& snip = snippetPart->second;

  if (!restrictedShards.empty()) {
    // Restricted case, store the restriction for the snippet.
    bool& isRestricted = snip.isRestricted;
    std::unordered_set<ShardID>& shards = snip.restrictedShards;
    if (isRestricted) {
      // We are already restricted, only possible if the restriction is
      // identical
      TRI_ASSERT(shards == restrictedShards);
      if (shards != restrictedShards) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_INTERNAL,
            "Restricted a snippet to two distinct shards of a collection.");
      }
    } else {
      isRestricted = true;
      for (auto const& s : restrictedShards) {
        if (!info.allShards.contains(s)) {
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
              absl::StrCat(
                  "Restricting: ", col->name(), " to shard ", std::string(s),
                  " which it does not have, or is excluded in the query"));
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
      for (auto const& sid : lockInfo.second.allShards) {
        auto server = shardMapping.find(sid);
        if (server != shardMapping.end()) {
          // We will create all maps as empty default constructions on the way
          _serverToCollectionToShard[server->second][lockInfo.first].emplace(
              sid);
          _serverToLockTypeToShard[server->second][lockInfo.second.lockType]
              .emplace(sid);
        }
      }
    }
    // We now have sorted out all participating servers. Insert Satellites
    for (auto& lockInfo : _collectionLocking) {
      if (lockInfo.second.isSatellite) {
        TRI_ASSERT(lockInfo.second.allShards.size() == 1);
        for (auto const& shard : lockInfo.second.allShards) {
          for (auto& serverLock : _serverToLockTypeToShard) {
            // For every server, add it! (using the given lock)
            serverLock.second[lockInfo.second.lockType].emplace(shard);
          }
        }
      }
    }
  }
  std::vector<ServerID> result;
  std::transform(_serverToCollectionToShard.begin(),
                 _serverToCollectionToShard.end(), std::back_inserter(result),
                 [](auto const& item) {
                   TRI_ASSERT(!item.first.empty());
                   return item.first;
                 });
  return result;
}

void ShardLocking::serializeIntoBuilder(
    ServerID const& server, arangodb::velocypack::Builder& builder) const {
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

containers::FlatHashMap<ShardID, ServerID> const&
ShardLocking::getShardMapping() {
  if (_shardMapping.empty() && !_collectionLocking.empty()) {
    containers::FlatHashSet<ShardID> shardIds;
    for (auto const& lockInfo : _collectionLocking) {
      auto& allShards = lockInfo.second.allShards;
      TRI_ASSERT(!allShards.empty());
      for (auto const& rest : lockInfo.second.snippetInfo) {
        if (!rest.second.isRestricted) {
          // We have an unrestricted snippet for this collection
          // Use all shards for locking
          for (auto const& s : allShards) {
            shardIds.emplace(s);
          }
          // No need to search further
          break;
        }
        for (auto const& s : rest.second.restrictedShards) {
          shardIds.emplace(s);
        }
      }
    }
    TRI_ASSERT(!shardIds.empty());
    auto& server = _query.vocbase().server();
    if (!server.hasFeature<ClusterFeature>()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }
    auto& cf = server.getFeature<ClusterFeature>();
    auto& ci = cf.clusterInfo();
#ifdef USE_ENTERPRISE
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    auto& trx = _query.trxForOptimization();
    if (trx.state()->options().allowDirtyReads) {
      ++cf.dirtyReadQueriesCounter();
      _shardMapping = trx.state()->whichReplicas(shardIds);
    } else
#endif
    {
      // We have at least one shard, otherwise we would not have snippets!
      _shardMapping = ci.getResponsibleServers(shardIds);
    }
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

std::unordered_set<ShardID> const& ShardLocking::shardsForSnippet(
    QuerySnippet::Id snippetId, Collection const* col) const {
  auto const& lockInfo = _collectionLocking.find(col);

  TRI_ASSERT(lockInfo != _collectionLocking.end());
  if (lockInfo == _collectionLocking.end()) {
    // We ask for a collection we did not lock!
    return EmptyShardListUnordered;
  }
  auto restricted = lockInfo->second.snippetInfo.find(snippetId);
  // We are asking for shards of a collection that are not registered with this
  TRI_ASSERT(restricted != lockInfo->second.snippetInfo.end());
  if (restricted == lockInfo->second.snippetInfo.end()) {
    return EmptyShardListUnordered;
  }
  if (restricted->second.isRestricted) {
    return restricted->second.restrictedShards;
  }
  return lockInfo->second.allShards;
}
