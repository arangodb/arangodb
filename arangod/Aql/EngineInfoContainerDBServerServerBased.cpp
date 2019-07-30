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

#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionNode.h"
#include "Aql/GraphNode.h"
#include "Aql/Query.h"
#include "Aql/QuerySnippet.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Graph/BaseOptions.h"
#include "StorageEngine/TransactionState.h"

#include <set>

using namespace arangodb;
using namespace arangodb::aql;

namespace {
const double SETUP_TIMEOUT = 90.0;

Result ExtractRemoteAndShard(VPackSlice keySlice, size_t& remoteId, std::string& shardId) {
  TRI_ASSERT(keySlice.isString());  // used as  a key in Json
  arangodb::velocypack::StringRef key(keySlice);
  size_t p = key.find(':');
  if (p == std::string::npos) {
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unexpected response from DBServer during setup"};
  }
  arangodb::velocypack::StringRef remId = key.substr(0, p);
  remoteId = basics::StringUtils::uint64(remId.begin(), remId.length());
  if (remoteId == 0) {
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unexpected response from DBServer during setup"};
  }
  shardId = key.substr(p + 1).toString();
  if (shardId.empty()) {
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unexpected response from DBServer during setup"};
  }
  return {TRI_ERROR_NO_ERROR};
}
}  // namespace

EngineInfoContainerDBServerServerBased::EngineInfoContainerDBServerServerBased(Query* query) noexcept
    : _query(query), _shardLocking(query) {}

// Insert a new node into the last engine on the stack
// If this Node contains Collections, they will be added into the map
// for ShardLocking
void EngineInfoContainerDBServerServerBased::addNode(ExecutionNode* node) {
  TRI_ASSERT(node);
  TRI_ASSERT(!_snippetStack.empty());
  // Add the node to the open Snippet
  _snippetStack.top()->addNode(node);
  // Upgrade CollectionLocks if necessary
  _shardLocking.addNode(node);
}

// Open a new snippet, which provides data for the given sink node (for now only RemoteNode allowed)
void EngineInfoContainerDBServerServerBased::openSnippet(size_t idOfSinkNode) {
  _snippetStack.emplace(std::make_shared<QuerySnippet>(idOfSinkNode));
}

// Closes the given snippet and connects it
// to the given queryid of the coordinator.
void EngineInfoContainerDBServerServerBased::closeSnippet(QueryId inputSnippet) {
  TRI_ASSERT(!_snippetStack.empty());
  auto e = _snippetStack.top();
  TRI_ASSERT(e);
  _snippetStack.pop();
  e->useQueryIdAsInput(inputSnippet);
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
Result EngineInfoContainerDBServerServerBased::buildEngines(MapRemoteToSnippet& queryIds) {
  // This needs to be a set with a defined order, it is important, that we contact
  // the database servers only in this specific order to avoid cluster-wide deadlock situations.
  std::vector<ServerID> dbServers = _shardLocking.getRelevantServers();

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
    addLockingPart(infoBuilder, server);
    TRI_ASSERT(infoBuilder.isOpenObject());

    addOptionsPart(infoBuilder, server);
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

    // add the transaction ID header
    std::unordered_map<std::string, std::string> headers;
    ClusterTrxMethods::addAQLTransactionHeader(*trx, server, headers);

    CoordTransactionID coordTransactionID = TRI_NewTickServer();
    auto res = cc->syncRequest(coordTransactionID, serverDest, RequestType::POST,
                               url, infoBuilder.toJson(), headers, SETUP_TIMEOUT);
    _query->incHttpRequests(1);
    if (res->getErrorCode() != TRI_ERROR_NO_ERROR) {
      LOG_TOPIC("f9a77", DEBUG, Logger::AQL)
          << server << " responded with " << res->getErrorCode() << " -> "
          << res->stringifyErrorMessage();
      LOG_TOPIC("41082", TRACE, Logger::AQL) << infoBuilder.toJson();
      return {res->getErrorCode(), res->stringifyErrorMessage()};
    }
    std::shared_ptr<VPackBuilder> builder = res->result->getBodyVelocyPack();
    VPackSlice response = builder->slice();
    auto result = parseResponse(response, queryIds, server, serverDest);
    if (!result.ok()) {
      return result;
    }
  }
  cleanupGuard.cancel();
  return TRI_ERROR_NO_ERROR;
}

Result EngineInfoContainerDBServerServerBased::parseResponse(
    VPackSlice response, MapRemoteToSnippet& queryIds, ServerID const& server,
    std::string const& serverDest) const {
  if (!response.isObject() || !response.get("result").isObject()) {
    LOG_TOPIC("0c3f2", ERR, Logger::AQL) << "Received error information from "
                                         << server << " : " << response.toJson();
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unable to deploy query on all required "
            "servers. This can happen during "
            "failover. Please check: " +
                server};
  }

  VPackSlice result = response.get("result");
  VPackSlice snippets = result.get("snippets");

  // Link Snippets to their sinks
  for (auto const& resEntry : VPackObjectIterator(snippets)) {
    if (!resEntry.value.isString()) {
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "Unable to deploy query on all required "
              "servers. This can happen during "
              "failover. Please check: " +
                  server};
    }
    size_t remoteId = 0;
    std::string shardId = "";
    auto res = ExtractRemoteAndShard(resEntry.key, remoteId, shardId);
    if (!res.ok()) {
      return res;
    }
    TRI_ASSERT(remoteId != 0);
    TRI_ASSERT(!shardId.empty());
    auto& remote = queryIds[remoteId];
    auto& thisServer = remote[serverDest];
    thisServer.emplace_back(resEntry.value.copyString());
  }

  // Link traverser engines to their nodes
  VPackSlice travEngines = result.get("traverserEngines");
  if (!travEngines.isNone()) {
    if (!travEngines.isArray()) {
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "Unable to deploy query on all required "
              "servers. This can happen during "
              "failover. Please check: " +
                  server};
    }
    if (travEngines.length() != _traverserEngineInfos.size()) {
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "The DBServer was not able to create enough "
              "traversal engines. This can happen during "
              "failover. Please check; " +
                  server};
    }
    auto idIter = VPackArrayIterator(travEngines);
    for (auto const& it : _traverserEngineInfos) {
      it.first->addEngine(idIter.value().getNumber<traverser::TraverserEngineID>(), server);
      idIter.next();
    }
  }
  return {TRI_ERROR_NO_ERROR};
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
 * @param queryIds A map of QueryIds of the format: (remoteNodeId:shardId)
 * -> queryid.
 */
void EngineInfoContainerDBServerServerBased::cleanupEngines(
    std::shared_ptr<ClusterComm> cc, int errorCode, std::string const& dbname,
    MapRemoteToSnippet& queryIds) const {}

// Insert a GraphNode that needs to generate TraverserEngines on
// the DBServers. The GraphNode itself will retain on the coordinator.
void EngineInfoContainerDBServerServerBased::addGraphNode(GraphNode* node) {
  _shardLocking.addNode(node);
  _graphNodes.emplace_back(node);
}

// Insert the Locking information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addLockingPart(arangodb::velocypack::Builder& builder,
                                                            ServerID const& server) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("lockInfo"));
  builder.openObject();
  _shardLocking.serializeIntoBuilder(server, builder);
  builder.close();  // lockInfo
}

// Insert the Options information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addOptionsPart(arangodb::velocypack::Builder& builder,
                                                            ServerID const& server) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("options"));
  // toVelocyPack will open & close the "options" object
#ifdef USE_ENTERPRISE
  if (_query->trx()->state()->options().skipInaccessibleCollections) {
    aql::QueryOptions opts = _query->queryOptions();
    TRI_ASSERT(opts.transactionOptions.skipInaccessibleCollections);
    auto usedCollections = _shardLocking.getUsedCollections();
    for (auto const& it : usedCollections) {
      TRI_ASSERT(it != nullptr);
      if (_query->trx()->isInaccessibleCollectionId(it->getPlanId())) {
        for (ShardID const& sid : _shardLocking.getShardsForCollection(server, it)) {
          opts.inaccessibleCollections.insert(sid);
        }
        opts.inaccessibleCollections.insert(std::to_string(it->getPlanId()));
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
    snippet->serializeIntoBuilder(server, builder);
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