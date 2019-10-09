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
#include "Basics/StringUtils.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Graph/BaseOptions.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "StorageEngine/TransactionState.h"
#include "Utils/CollectionNameResolver.h"

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

EngineInfoContainerDBServerServerBased::TraverserEngineShardLists::TraverserEngineShardLists(
    GraphNode const* node, ServerID const& server,
    std::unordered_map<ShardID, ServerID> const& shardMapping, Query const& query)
    : _node(node), _hasShard(false) {
  auto const& edges = _node->edgeColls();
  TRI_ASSERT(!edges.empty());
  std::unordered_set<std::string> const& restrictToShards =
      query.queryOptions().shardIds;
  // Extract the local shards for edge collections.
  for (auto const& col : edges) {
    _edgeCollections.emplace_back(
        getAllLocalShards(shardMapping, server, col->shardIds(restrictToShards)));
  }
  // Extract vertices
  auto const& vertices = _node->vertexColls();
  // Guaranteed by addGraphNode, this will inject vertex collections
  // in anonymous graph case
  // It might in fact be empty, if we only have edge collections in a graph.
  // Or if we guarantee to never read vertex data.
  for (auto const& col : vertices) {
    auto shards = getAllLocalShards(shardMapping, server, col->shardIds(restrictToShards));
#ifdef USE_ENTERPRISE
    for (auto const& s : shards) {
      if (query.trx()->isInaccessibleCollectionId(col->getPlanId())) {
        _inaccessibleShards.insert(s);
        _inaccessibleShards.insert(std::to_string(col->id()));
      }
    }
#endif
    _vertexCollections.emplace(col->name(), std::move(shards));
  }
}

std::vector<ShardID> EngineInfoContainerDBServerServerBased::TraverserEngineShardLists::getAllLocalShards(
    std::unordered_map<ShardID, ServerID> const& shardMapping,
    ServerID const& server, std::shared_ptr<std::vector<std::string>> shardIds) {
  std::vector<ShardID> localShards;
  for (auto const& shard : *shardIds) {
    auto const& it = shardMapping.find(shard);
    TRI_ASSERT(it != shardMapping.end());
    if (it->second == server) {
      localShards.emplace_back(shard);
      _hasShard = true;
    }
  }
  return localShards;
}

void EngineInfoContainerDBServerServerBased::TraverserEngineShardLists::serializeIntoBuilder(
    VPackBuilder& infoBuilder) const {
  TRI_ASSERT(_hasShard);
  TRI_ASSERT(infoBuilder.isOpenArray());
  infoBuilder.openObject();
  {
    // Options
    infoBuilder.add(VPackValue("options"));
    graph::BaseOptions* opts = _node->options();
    opts->buildEngineInfo(infoBuilder);
  }
  {
    // Variables
    std::vector<aql::Variable const*> vars;
    _node->getConditionVariables(vars);
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
  for (auto const& col : _vertexCollections) {
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
  for (auto const& edgeShards : _edgeCollections) {
    infoBuilder.openArray();
    for (auto const& e : edgeShards) {
      infoBuilder.add(VPackValue(e));
    }
    infoBuilder.close();
  }
  infoBuilder.close();  // edges

#ifdef USE_ENTERPRISE
  if (!_inaccessibleShards.empty()) {
    infoBuilder.add(VPackValue("inaccessible"));
    infoBuilder.openArray();
    for (ShardID const& shard : _inaccessibleShards) {
      infoBuilder.add(VPackValue(shard));
    }
    infoBuilder.close();  // inaccessible
  }
#endif
  infoBuilder.close();  // shards

  _node->enhanceEngineInfo(infoBuilder);

  infoBuilder.close();  // base
  TRI_ASSERT(infoBuilder.isOpenArray());
}

EngineInfoContainerDBServerServerBased::EngineInfoContainerDBServerServerBased(Query& query) noexcept
    : _query(query), _shardLocking(&query), _lastSnippetId(1) {
  // NOTE: We need to start with _lastSnippetID > 0. 0 is reserved for GraphNodes
}

void EngineInfoContainerDBServerServerBased::injectVertexColletions(GraphNode* graphNode){
    auto const& vCols = graphNode->vertexColls();
    if (vCols.empty()) {
      std::map<std::string, Collection*> const* allCollections =
          _query.collections()->collections();
      auto& resolver = _query.resolver();
      for (auto const& it : *allCollections) {
        // If resolver cannot resolve this collection
        // it has to be a view.
        if (!resolver.getCollection(it.first)) {
          continue;
        }
        // All known edge collections will be ignored by this call!
        graphNode->injectVertexCollection(it.second);
      }
    }
}

// Insert a new node into the last engine on the stack
// If this Node contains Collections, they will be added into the map
// for ShardLocking
void EngineInfoContainerDBServerServerBased::addNode(ExecutionNode* node) {
  TRI_ASSERT(node);
  TRI_ASSERT(!_snippetStack.empty());

  // Add the node to the open Snippet
  _snippetStack.top()->addNode(node);

  switch (node->getType()) {
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS: {
      injectVertexColletions(static_cast<GraphNode*>(node));
      break;
    }
    default:
      break;
  }

  // Upgrade CollectionLocks if necessary
  _shardLocking.addNode(node, _snippetStack.top()->id());
}

// Open a new snippet, which provides data for the given sink node (for now only RemoteNode allowed)
void EngineInfoContainerDBServerServerBased::openSnippet(GatherNode const* sinkGatherNode,
                                                         size_t sinkRemoteId) {
  _snippetStack.emplace(std::make_shared<QuerySnippet>(sinkGatherNode, sinkRemoteId,
                                                       _lastSnippetId++));
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
Result EngineInfoContainerDBServerServerBased::buildEngines(
    MapRemoteToSnippet& queryIds, std::unordered_map<size_t, size_t>& nodeAliases) {
  // This needs to be a set with a defined order, it is important, that we contact
  // the database servers only in this specific order to avoid cluster-wide deadlock situations.
  std::vector<ServerID> dbServers = _shardLocking.getRelevantServers();
  if (dbServers.empty()) {
    // No snippets to be placed on dbservers
    return TRI_ERROR_NO_ERROR;
  }
  // We at least have one Snippet, or one graph node.
  // Otherwise the locking needs to be empty.
  TRI_ASSERT(!_closedSnippets.empty() || !_graphNodes.empty());

  NetworkFeature const& nf = _query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  double ttl = _query.queryOptions().ttl;

  std::string const url(
      "/_db/" + arangodb::basics::StringUtils::urlEncode(_query.vocbase().name()) +
      "/_api/aql/setup?ttl=" + std::to_string(ttl));

  auto cleanupGuard = scopeGuard([this, pool, &queryIds]() {
    cleanupEngines(pool, TRI_ERROR_INTERNAL, _query.vocbase().name(), queryIds);
  });

  // Build Lookup Infos
  VPackBuilder infoBuilder;
  transaction::Methods* trx = _query.trx();
  network::RequestOptions options;
  options.timeout = network::Timeout(SETUP_TIMEOUT);

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

    addSnippetPart(infoBuilder, _shardLocking, nodeAliases, server);
    TRI_ASSERT(infoBuilder.isOpenObject());
    auto shardMapping = _shardLocking.getShardMapping();
    std::vector<bool> didCreateEngine =
        addTraversalEnginesPart(infoBuilder, shardMapping, server);
    TRI_ASSERT(didCreateEngine.size() == _graphNodes.size());
    TRI_ASSERT(infoBuilder.isOpenObject());

    infoBuilder.close();  // Base object
    TRI_ASSERT(infoBuilder.isClosed());

    VPackSlice infoSlice = infoBuilder.slice();
    // Partial assertions to check if all required keys are present
    TRI_ASSERT(infoSlice.hasKey("lockInfo"));
    TRI_ASSERT(infoSlice.hasKey("options"));
    TRI_ASSERT(infoSlice.hasKey("variables"));
    // We need to have at least one: snippets (non empty) or traverserEngines

    if (!((infoSlice.hasKey("snippets") && infoSlice.get("snippets").isObject() &&
           !infoSlice.get("snippets").isEmptyObject()) ||
          infoSlice.hasKey("traverserEngines"))) {
      // This is possible in the satellite case.
      // The leader of a read-only satellite is potentially
      // not part of the query.
      continue;
    }
    TRI_ASSERT((infoSlice.hasKey("snippets") && infoSlice.get("snippets").isObject() &&
                !infoSlice.get("snippets").isEmptyObject()) ||
               infoSlice.hasKey("traverserEngines"));

    VPackBuffer<uint8_t> buffer(infoSlice.byteSize());
    buffer.append(infoSlice.begin(), infoSlice.byteSize());

    // add the transaction ID header
    network::Headers headers;
    ClusterTrxMethods::addAQLTransactionHeader(*trx, server, headers);
    auto res = network::sendRequest(pool, serverDest, fuerte::RestVerb::Post,
                                    url, std::move(buffer), headers, options)
                   .get();
    _query.incHttpRequests(1);
    if (res.fail()) {
      int code = network::fuerteToArangoErrorCode(res);
      std::string message = network::fuerteToArangoErrorMessage(res);
      LOG_TOPIC("f9a77", DEBUG, Logger::AQL)
          << server << " responded with " << code << " -> " << message;
      LOG_TOPIC("41082", TRACE, Logger::AQL) << infoSlice.toJson();
      return {code, message};
    }
    auto slices = res.response->slices();
    if (slices.empty()) {
      return {TRI_ERROR_INTERNAL, "malformed response while building engines"};
    }
    VPackSlice response = slices[0];
    auto result = parseResponse(response, queryIds, server, serverDest, didCreateEngine);
    if (!result.ok()) {
      return result;
    }
  }
  cleanupGuard.cancel();
  return TRI_ERROR_NO_ERROR;
}

Result EngineInfoContainerDBServerServerBased::parseResponse(
    VPackSlice response, MapRemoteToSnippet& queryIds, ServerID const& server,
    std::string const& serverDest, std::vector<bool> const& didCreateEngine) const {
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
    auto idIter = VPackArrayIterator(travEngines);
    TRI_ASSERT(_graphNodes.size() == didCreateEngine.size());
    for (size_t i = 0; i < _graphNodes.size(); ++i) {
      if (didCreateEngine[i]) {
        if (!idIter.valid()) {
          return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
                  "The DBServer was not able to create enough "
                  "traversal engines. This can happen during "
                  "failover. Please check; " +
                      server};
        }
        _graphNodes[i]->addEngine(idIter.value().getNumber<traverser::TraverserEngineID>(),
                                  server);
        idIter.next();
      }
    }
    // We need to consume all traverser engines
    TRI_ASSERT(!idIter.valid());
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
 * @param pool The ConnectionPool
 * @param errorCode error Code to be send to DBServers for logging.
 * @param dbname Name of the database this query is executed in.
 * @param queryIds A map of QueryIds of the format: (remoteNodeId:shardId)
 * -> queryid.
 */
void EngineInfoContainerDBServerServerBased::cleanupEngines(
    network::ConnectionPool* pool, int errorCode, std::string const& dbname,
    MapRemoteToSnippet& queryIds) const {
  network::RequestOptions options;
  options.timeout = network::Timeout(10.0);  // Picked arbitrarily
  network::Headers headers;

  // Shutdown query snippets
  std::string url("/_db/" + arangodb::basics::StringUtils::urlEncode(dbname) +
                  "/_api/aql/shutdown/");
  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.openObject();
  builder.add("code", VPackValue(std::to_string(errorCode)));
  builder.close();
  for (auto const& it : queryIds) {
    // it.first == RemoteNodeId, we don't need this
    // it.second server -> [snippets]
    for (auto const& serToSnippets : it.second) {
      auto server = serToSnippets.first;
      for (auto const& shardId : serToSnippets.second) {
        // fire and forget
        network::sendRequest(pool, server, fuerte::RestVerb::Put, url + shardId,
                             body, headers, options);
      }
      _query.incHttpRequests(serToSnippets.second.size());
    }
  }

  // Shutdown traverser engines
  url = "/_db/" + arangodb::basics::StringUtils::urlEncode(dbname) +
        "/_internal/traverser/";
  VPackBuffer<uint8_t> noBody;

  for (auto const& gn : _graphNodes) {
    auto allEngines = gn->engines();
    for (auto const& engine : *allEngines) {
      // fire and forget
      network::sendRequestRetry(pool, engine.first, fuerte::RestVerb::Delete,
                                url + basics::StringUtils::itoa(engine.second),
                                noBody, headers, options);
    }
    _query.incHttpRequests(allEngines->size());
  }

  queryIds.clear();
}

// Insert a GraphNode that needs to generate TraverserEngines on
// the DBServers. The GraphNode itself will retain on the coordinator.
void EngineInfoContainerDBServerServerBased::addGraphNode(GraphNode* node) {
  node->prepareOptions();
  injectVertexColletions(node);
  // SnippetID does not matter on GraphNodes
  _shardLocking.addNode(node, 0);
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
  if (_query.trx()->state()->options().skipInaccessibleCollections) {
    aql::QueryOptions opts = _query.queryOptions();
    TRI_ASSERT(opts.transactionOptions.skipInaccessibleCollections);
    auto usedCollections = _shardLocking.getUsedCollections();
    for (auto const& it : usedCollections) {
      TRI_ASSERT(it != nullptr);
      if (_query.trx()->isInaccessibleCollectionId(it->getPlanId())) {
        for (ShardID const& sid : _shardLocking.getShardsForCollection(server, it)) {
          opts.inaccessibleCollections.insert(sid);
        }
        opts.inaccessibleCollections.insert(std::to_string(it->getPlanId()));
      }
    }
    opts.toVelocyPack(builder, true);
  } else {
    _query.queryOptions().toVelocyPack(builder, true);
  }
#else
  _query.queryOptions().toVelocyPack(builder, true);
#endif
}

// Insert the Variables information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addVariablesPart(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("variables"));
  // This will open and close an Object.
  _query.ast()->variables()->toVelocyPack(builder);
}

// Insert the Snippets information into the message to be send to DBServers
void EngineInfoContainerDBServerServerBased::addSnippetPart(
    arangodb::velocypack::Builder& builder, ShardLocking& shardLocking,
    std::unordered_map<size_t, size_t>& nodeAliases, ServerID const& server) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("snippets"));
  builder.openObject();
  for (auto const& snippet : _closedSnippets) {
    snippet->serializeIntoBuilder(server, shardLocking, nodeAliases, builder);
  }
  builder.close();  // snippets
}

// Insert the TraversalEngine information into the message to be send to DBServers
std::vector<bool> EngineInfoContainerDBServerServerBased::addTraversalEnginesPart(
    arangodb::velocypack::Builder& infoBuilder,
    std::unordered_map<ShardID, ServerID> const& shardMapping, ServerID const& server) const {
  std::vector<bool> result;
  if (_graphNodes.empty()) {
    return result;
  }
  result.reserve(_graphNodes.size());
  TRI_ASSERT(infoBuilder.isOpenObject());
  infoBuilder.add(VPackValue("traverserEngines"));
  infoBuilder.openArray();
  for (auto const* graphNode : _graphNodes) {
    TraverserEngineShardLists list(graphNode, server, shardMapping, _query);
    if (list.hasShard()) {
      list.serializeIntoBuilder(infoBuilder);
    }
    // If we have a shard, we expect to get a response back for this engine
    result.emplace_back(list.hasShard());
    TRI_ASSERT(infoBuilder.isOpenArray());
  }
  infoBuilder.close();  // traverserEngines
  return result;
}
