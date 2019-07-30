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

#include "EngineInfoContainerDBServerServerBased.h"

#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/IResearchViewNode.h"
#include "Aql/ModificationNodes.h"
#include "Aql/Query.h"
#include "Aql/QuerySnippet.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Graph/BaseOptions.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"

#include <set>

using namespace arangodb;
using namespace arangodb::aql;

void EngineInfoContainerDBServerServerBased::CollectionLockingInformation::mergeShards(
    std::shared_ptr<std::vector<ShardID>> const& shards) {
  for (auto const& s : *shards) {
    usedShards.emplace(s);
  }
}

EngineInfoContainerDBServerServerBased::EngineInfoContainerDBServerServerBased(Query* query) noexcept
    : _query(query) {}

// Insert a new node into the last engine on the stack
// If this Node contains Collections, they will be added into the map
// for ShardLocking
void EngineInfoContainerDBServerServerBased::addNode(ExecutionNode* node) {
  TRI_ASSERT(node);
  TRI_ASSERT(!_snippetStack.empty());
  // Add the node to the open Snippet
  _snippetStack.top()->addNode(node);
  // Upgrade CollectionLocks if necessary
  handleCollectionLocking(node);
}

// Open a new snippet, which provides data for the given sink node (for now only RemoteNode allowed)
void EngineInfoContainerDBServerServerBased::openSnippet(size_t idOfSinkNode) {
  _snippetStack.emplace(std::make_shared<QuerySnippet>(idOfSinkNode));
}

// Closes the given snippet and connects it
// to the given queryid of the coordinator.
void EngineInfoContainerDBServerServerBased::closeSnippet(QueryId id) {
  TRI_ASSERT(!_snippetStack.empty());
  auto e = _snippetStack.top();
  TRI_ASSERT(e);
  _snippetStack.pop();
  e->connectToQueryId(id);
  _closedSnippets.emplace_back(std::move(e));
}

// Build the Engines for the DBServer
//   * Creates one Query-Entry for each Snippet per Shard (multiple on the
//   same DB)
//   * All snippets know all locking information for the query.
//   * Only the first snippet is responsible to lock.
//   * After this step DBServer-Collections are locked!
//
//   Error Case: It is guaranteed that for all snippets created during
//   this methods a shutdown request is send to all DBServers.
//   In case the network is broken and this shutdown request is lost
//   the DBServers will clean up their snippets after a TTL.
Result EngineInfoContainerDBServerServerBased::buildEngines(MapRemoteToSnippet& queryIds) const {
  // This needs to be a set with a defined order, it is important, that we contact
  // the database servers only in this specific order to avoid cluster-wide deadlock situations.
  std::set<ServerID> dbServers;

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  double ttl = _query->queryOptions().ttl;

  std::string const url(
      "/_db/" + arangodb::basics::StringUtils::urlEncode(_query->vocbase().name()) +
      "/_api/aql/setup?ttl=" + std::to_string(ttl));

  auto cleanupGuard = scopeGuard([this, &cc, &queryIds]() {
    cleanupEngines(cc, TRI_ERROR_INTERNAL, _query->vocbase().name(), queryIds);
  });

  // TODO Figure out which servers participate

  // Build Lookup Infos
  VPackBuilder infoBuilder;
  transaction::Methods* trx = _query->trx();
  for (auto const& server : dbServers) {
    std::string const serverDest = "server:" + server;

    LOG_TOPIC("4bbe6", DEBUG, arangodb::Logger::AQL)
        << "Building Engine Info for " << server;
    infoBuilder.clear();
    infoBuilder.openObject();
    addLockingPart(infoBuilder);
    TRI_ASSERT(infoBuilder.isOpenObject());

    addOptionsPart(infoBuilder);
    TRI_ASSERT(infoBuilder.isOpenObject());

    addVariablesPart(infoBuilder);
    TRI_ASSERT(infoBuilder.isOpenObject());

    addSnippetPart(infoBuilder, server);
    TRI_ASSERT(infoBuilder.isOpenObject());

    addTraversalEnginesPart(infoBuilder, server);
    TRI_ASSERT(infoBuilder.isOpenObject());

    infoBuilder.close();  // Base object
    TRI_ASSERT(infoBuilder.isClosed());
    // Partial assertions to check if all required keys are present
    TRI_ASSERT(infoBuilder.slice().hasKey("lockInfo"));
    TRI_ASSERT(infoBuilder.slice().hasKey("options"));
    TRI_ASSERT(infoBuilder.slice().hasKey("variables"));
    // We need to have at least one: snippets or traverserEngines
    TRI_ASSERT(infoBuilder.slice().hasKey("snippets") ||
               infoBuilder.slice().hasKey("traverserEngines"));
    // TODO send messages out per server
  }

  return TRI_ERROR_NO_ERROR;
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
void EngineInfoContainerDBServerServerBased::cleanupEngines(
    std::shared_ptr<ClusterComm> cc, int errorCode, std::string const& dbname,
    MapRemoteToSnippet& queryIds) const {}

// Insert a GraphNode that needs to generate TraverserEngines on
// the DBServers. The GraphNode itself will retain on the coordinator.
void EngineInfoContainerDBServerServerBased::addGraphNode(GraphNode* node) {
  handleCollectionLocking(node);
  _graphNodes.emplace_back(node);
}

void EngineInfoContainerDBServerServerBased::handleCollectionLocking(ExecutionNode const* baseNode) {
  TRI_ASSERT(baseNode != nullptr);
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

void EngineInfoContainerDBServerServerBased::updateLocking(
    Collection const* col, AccessMode::Type const& accessType,
    std::unordered_set<std::string> const& restrictedShards) {
  auto const shards = col->shardIds(
      restrictedShards.empty() ? _query->queryOptions().shardIds : restrictedShards);

  // What if we have an empty shard list here?
  if (shards->empty()) {
    // TODO FIXME
    LOG_TOPIC("0997e", WARN, arangodb::Logger::AQL)
        << "TEMPORARY: A collection access of a query has no result in any "
           "shard";
  }

  auto& info = _collectionLocking[col];
  // We need to upgrade the lock
  info.lockType = std::max(info.lockType, accessType);
  info.mergeShards(shards);
}

// Insert the Locking information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addLockingPart(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("lockInfo"));
  builder.openObject();
  // TODO insert
  builder.close();  // lockInfo
}

// Insert the Options information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addOptionsPart(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("options"));
  // toVelocyPack will open & close the "options" object
#ifdef USE_ENTERPRISE
  if (_query->trx()->state()->options().skipInaccessibleCollections) {
    aql::QueryOptions opts = _query->queryOptions();
    TRI_ASSERT(opts.transactionOptions.skipInaccessibleCollections);
    for (auto const& it : _collectionLocking) {
      TRI_ASSERT(it.first);
      if (_query->trx()->isInaccessibleCollectionId(it.first->getPlanId())) {
        for (ShardID const& sid : it.second.usedShards) {
          opts.inaccessibleCollections.insert(sid);
        }
        opts.inaccessibleCollections.insert(std::to_string(it.first->getPlanId()));
      }
    }
    opts.toVelocyPack(builder, true);
  } else {
    _query->queryOptions().toVelocyPack(builder, true);
  }
#else
  _query->queryOptions().toVelocyPack(builder, true);
#endif
}

// Insert the Variables information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addVariablesPart(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("variables"));
  // This will open and close an Object.
  _query->ast()->variables()->toVelocyPack(builder);
}

// Insert the Snippets information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addSnippetPart(arangodb::velocypack::Builder& builder,
                                                            ServerID const& server) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("snippets"));
  builder.openArray();
  for (auto const& snippet : _closedSnippets) {
    snippet.serializeIntoBuilder(server, builder);
  }
  builder.close();  // snippets
}

// Insert the TraversalEngine information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addTraversalEnginesPart(
    arangodb::velocypack::Builder& infoBuilder, ServerID const& server) const {
  if (_traverserEngineInfos.empty()) {
    return;
  }
  TRI_ASSERT(infoBuilder.isOpenObject());
  infoBuilder.add(VPackValue("traverserEngines"));
  infoBuilder.openArray();
  for (auto const& it : _traverserEngineInfos) {
    GraphNode* en = it.first;
    TraverserEngineShardLists const& list = it.second;
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

#ifdef USE_ENTERPRISE
    if (!list.inaccessibleShards.empty()) {
      infoBuilder.add(VPackValue("inaccessible"));
      infoBuilder.openArray();
      for (ShardID const& shard : list.inaccessibleShards) {
        infoBuilder.add(VPackValue(shard));
      }
      infoBuilder.close();  // inaccessible
    }
#endif
    infoBuilder.close();  // shards

    en->enhanceEngineInfo(infoBuilder);

    infoBuilder.close();  // base
  }

  infoBuilder.close();  // traverserEngines
}