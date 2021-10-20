////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/GraphNode.h"
#include "Aql/TraverserEngineShardLists.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterTrxMethods.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Random/RandomGenerator.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Manager.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Collection.h>
#include <set>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::basics;

namespace {
const double SETUP_TIMEOUT = 60.0;
// Wait 2s to get the Lock in FastPath, otherwise assume dead-lock.
const double FAST_PATH_LOCK_TIMEOUT = 2.0;

std::string const finishUrl("/_api/aql/finish/");
std::string const traverserUrl("/_internal/traverser/");

Result ExtractRemoteAndShard(VPackSlice keySlice, ExecutionNodeId& remoteId,
                             std::string& shardId) {
  TRI_ASSERT(keySlice.isString());  // used as  a key in Json
  arangodb::velocypack::StringRef key(keySlice);
  size_t p = key.find(':');
  if (p == std::string::npos) {
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unexpected response from DBServer during setup"};
  }
  arangodb::velocypack::StringRef remId = key.substr(0, p);
  remoteId = ExecutionNodeId{basics::StringUtils::uint64(remId.begin(), remId.length())};
  if (remoteId == ExecutionNodeId{0}) {
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

EngineInfoContainerDBServerServerBased::EngineInfoContainerDBServerServerBased(QueryContext& query) noexcept
    : _query(query), _shardLocking(query), _lastSnippetId(1) {
  // NOTE: We need to start with _lastSnippetID > 0. 0 is reserved for GraphNodes
}

void EngineInfoContainerDBServerServerBased::injectVertexCollections(GraphNode* graphNode) {
  auto const& vCols = graphNode->vertexColls();
  if (vCols.empty()) {
    auto& resolver = _query.resolver();
    _query.collections().visit([&resolver, graphNode](std::string const& name,
                                                      aql::Collection& collection) {
      // If resolver cannot resolve this collection
      // it has to be a view.
      if (resolver.getCollection(name)) {
        // All known edge collections will be ignored by this call!
        graphNode->injectVertexCollection(collection);
      }
      return true;
    });
  }
}

// Insert a new node into the last engine on the stack
// If this Node contains Collections, they will be added into the map
// for ShardLocking
void EngineInfoContainerDBServerServerBased::addNode(ExecutionNode* node, bool pushToSingleServer) {
  TRI_ASSERT(node);
  TRI_ASSERT(!_snippetStack.empty());

  // Add the node to the open Snippet
  _snippetStack.top()->addNode(node);

  switch (node->getType()) {
    case ExecutionNode::TRAVERSAL:
    case ExecutionNode::SHORTEST_PATH:
    case ExecutionNode::K_SHORTEST_PATHS: {
      auto* const graphNode = ExecutionNode::castTo<GraphNode*>(node);
      graphNode->prepareOptions();
      injectVertexCollections(graphNode);
      break;
    }
    default:
      break;
  }

  // Upgrade CollectionLocks if necessary
  _shardLocking.addNode(node, _snippetStack.top()->id(), pushToSingleServer);
}

// Open a new snippet, which provides data for the given sink node (for now only RemoteNode allowed)
void EngineInfoContainerDBServerServerBased::openSnippet(GatherNode const* sinkGatherNode,
                                                         ExecutionNodeId sinkRemoteId) {
  _snippetStack.emplace(std::make_shared<QuerySnippet>(sinkGatherNode, sinkRemoteId,
                                                       _lastSnippetId++));
}

// Closes the given snippet and connects it
// to the given queryid of the coordinator.
void EngineInfoContainerDBServerServerBased::closeSnippet(QueryId inputSnippet) {
  TRI_ASSERT(!_snippetStack.empty());
  std::shared_ptr<QuerySnippet> e = _snippetStack.top();
  TRI_ASSERT(e);
  _snippetStack.pop();
  e->useQueryIdAsInput(inputSnippet);
  _closedSnippets.emplace_back(std::move(e));
}

std::vector<bool> EngineInfoContainerDBServerServerBased::buildEngineInfo(
    QueryId clusterQueryId, VPackBuilder& infoBuilder, ServerID const& server,
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    std::map<ExecutionNodeId, ExecutionNodeId>& nodeAliases) {
  LOG_TOPIC("4bbe6", DEBUG, arangodb::Logger::AQL)
      << "Building Engine Info for " << server;

  infoBuilder.clear();
  infoBuilder.openObject();
  infoBuilder.add("clusterQueryId", VPackValue(clusterQueryId));

  addLockingPart(infoBuilder, server);
  TRI_ASSERT(infoBuilder.isOpenObject());

  addOptionsPart(infoBuilder, server);
  TRI_ASSERT(infoBuilder.isOpenObject());

  addVariablesPart(infoBuilder);
  TRI_ASSERT(infoBuilder.isOpenObject());

  infoBuilder.add("isModificationQuery", VPackValue(_query.isModificationQuery()));
  infoBuilder.add("isAsyncQuery", VPackValue(_query.isAsyncQuery()));

  infoBuilder.add(StaticStrings::AttrCoordinatorRebootId,
                  VPackValue(ServerState::instance()->getRebootId().value()));
  infoBuilder.add(StaticStrings::AttrCoordinatorId,
                  VPackValue(ServerState::instance()->getId()));

  addSnippetPart(nodesById, infoBuilder, _shardLocking, nodeAliases, server);
  TRI_ASSERT(infoBuilder.isOpenObject());
  auto shardMapping = _shardLocking.getShardMapping();
  std::vector<bool> didCreateEngine =
      addTraversalEnginesPart(infoBuilder, shardMapping, server);
  TRI_ASSERT(didCreateEngine.size() == _graphNodes.size());
  TRI_ASSERT(infoBuilder.isOpenObject());

  infoBuilder.add(StaticStrings::SerializationFormat,
                  VPackValue(static_cast<SerializationFormatType>(
                      aql::SerializationFormat::SHADOWROWS)));

  transaction::Methods& trx = _query.trxForOptimization();
  trx.state()->analyzersRevision().toVelocyPack(infoBuilder);
  infoBuilder.close();  // Base object
  TRI_ASSERT(infoBuilder.isClosed());

  return didCreateEngine;
}

arangodb::futures::Future<Result> EngineInfoContainerDBServerServerBased::buildSetupRequest(
    transaction::Methods& trx, ServerID const& server, VPackSlice infoSlice,
    std::vector<bool> didCreateEngine, MapRemoteToSnippet& snippetIds,
    aql::ServerQueryIdList& serverToQueryId, std::mutex& serverToQueryIdLock,
    network::ConnectionPool* pool, network::RequestOptions const& options) const {
  TRI_ASSERT(server.substr(0, 7) != "server:");

  VPackBuffer<uint8_t> buffer(infoSlice.byteSize());
  buffer.append(infoSlice.begin(), infoSlice.byteSize());

  // add the transaction ID header
  network::Headers headers;
  ClusterTrxMethods::addAQLTransactionHeader(trx, server, headers);

  TRI_ASSERT(infoSlice.isObject() && infoSlice.get("clusterQueryId").isUInt());
  QueryId globalId = infoSlice.get("clusterQueryId").getNumber<QueryId>();

  auto buildCallback =
      [this, server, didCreateEngine = std::move(didCreateEngine),
       &serverToQueryId, &serverToQueryIdLock, &snippetIds,
       globalId](arangodb::futures::Try<arangodb::network::Response> const& response) -> Result {
    auto const& resolvedResponse = response.get();
    auto queryId = globalId;
    RebootId rebootId{0};

    TRI_ASSERT(server.substr(0, 7) != "server:");

    std::unique_lock<std::mutex> guard{serverToQueryIdLock};

    if (resolvedResponse.fail()) {
      Result res = resolvedResponse.combinedResult();
      LOG_TOPIC("f9a77", DEBUG, Logger::AQL)
          << server << " responded with " << res.errorNumber() << ": "
          << res.errorMessage();

      serverToQueryId.emplace_back(ServerQueryIdEntry{server, globalId, rebootId});
      return res;
    }

    VPackSlice responseSlice = resolvedResponse.slice();
    if (responseSlice.isNone()) {
      return {TRI_ERROR_INTERNAL, "malformed response while building engines"};
    }
    auto result = parseResponse(responseSlice, snippetIds, server,
                                didCreateEngine, queryId, rebootId);
    serverToQueryId.emplace_back(ServerQueryIdEntry{server, queryId, rebootId});

    return result;
  };

  return network::sendRequestRetry(pool, "server:" + server,
                                   fuerte::RestVerb::Post, "/_api/aql/setup",
                                   std::move(buffer), options, std::move(headers))
      .then([buildCallback = std::move(buildCallback)](futures::Try<network::Response>&& resp) mutable {
        return buildCallback(resp);
      });
}

bool EngineInfoContainerDBServerServerBased::isNotSatelliteLeader(VPackSlice infoSlice) const {
  // Partial assertions to check if all required keys are present
  TRI_ASSERT(infoSlice.hasKey("lockInfo"));
  TRI_ASSERT(infoSlice.hasKey("options"));
  TRI_ASSERT(infoSlice.hasKey("variables"));
  // We need to have at least one: snippets (non empty) or traverserEngines

  if (!((infoSlice.get("snippets").isObject() && !infoSlice.get("snippets").isEmptyObject()) ||
        infoSlice.hasKey("traverserEngines"))) {
    // This is possible in the satellite case.
    // The leader of a read-only satellite is potentially
    // not part of the query.
    return true;
  }

  TRI_ASSERT((infoSlice.get("snippets").isObject() &&
              !infoSlice.get("snippets").isEmptyObject()) ||
             infoSlice.hasKey("traverserEngines"));

  return false;
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
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    MapRemoteToSnippet& snippetIds, aql::ServerQueryIdList& serverToQueryId,
    std::map<ExecutionNodeId, ExecutionNodeId>& nodeAliases) {
  // This needs to be a set with a defined order, it is important, that we contact
  // the database servers only in this specific order to avoid cluster-wide deadlock situations.
  std::vector<ServerID> dbServers = _shardLocking.getRelevantServers();
  if (dbServers.empty()) {
    // No snippets to be placed on dbservers
    return {};
  }
  // We at least have one Snippet, or one graph node.
  // Otherwise the locking needs to be empty.
  TRI_ASSERT(!_closedSnippets.empty() || !_graphNodes.empty());

  ErrorCode cleanupReason = TRI_ERROR_CLUSTER_TIMEOUT;
  
  auto cleanupGuard = scopeGuard([this, &serverToQueryId, &cleanupReason]() noexcept {
    try {
      transaction::Methods& trx = _query.trxForOptimization();
      auto requests = cleanupEngines(cleanupReason, _query.vocbase().name(), serverToQueryId);
      if (!trx.isMainTransaction()) {
        // for AQL queries in streaming transactions, we will wait for the
        // complete shutdown to have finished before we return to the caller.
        // this is done so that there will be no 2 AQL queries in the same
        // streaming transaction at the same time
        futures::collectAll(requests).wait();
      }
    } catch (std::exception const& ex) {
      LOG_TOPIC("2a9fe", WARN, Logger::AQL)
          << "unable to clean up query snippets: " << ex.what();
    }
  });

  NetworkFeature const& nf = _query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    // nullptr only happens on controlled shutdown
    return {TRI_ERROR_SHUTTING_DOWN};
  }

  double oldTtl = _query.queryOptions().ttl;
  // we use a timeout of at least the trx timeout for DB server snippets.
  // we assume this is safe because the RebootTracker on the coordinator
  // will abort all snippets of failed coordinators eventually.
  // the ttl on the coordinator is not that high, i.e. if an AQL query
  // is abandoned by a client application or an end user, the coordinator
  // ttl should kick in a lot earlier and also terminate the query on the
  // DB server(s).
  _query.queryOptions().ttl =
      std::max<double>(oldTtl, transaction::Manager::idleTTLDBServer);

  auto ttlGuard = scopeGuard([this, oldTtl]() noexcept {
    // restore previous TTL value
    _query.queryOptions().ttl = oldTtl;
  });

  // remember which servers we add during our setup request
  ::arangodb::containers::HashSet<std::string> serversAdded;

  transaction::Methods& trx = _query.trxForOptimization();
  std::vector<arangodb::futures::Future<Result>> networkCalls{};

  network::RequestOptions options;
  options.database = _query.vocbase().name();
  options.timeout = network::Timeout(SETUP_TIMEOUT);
  options.skipScheduler = true;  // hack to speed up future.get()

  TRI_IF_FAILURE("Query::setupTimeout") {
    options.timeout =
        network::Timeout(0.01 + (double)RandomGenerator::interval(uint32_t(10)));
  }

  TRI_IF_FAILURE("Query::setupTimeoutFailSequence") {
    double t = 0.5;
    TRI_IF_FAILURE("Query::setupTimeoutFailSequenceRandom") {
      if (RandomGenerator::interval(uint32_t(100)) >= 95) {
        t = 3.0;
      }
    }
    options.timeout = network::Timeout(t);
  }

  /// cluster global query id, under which the query will be registered
  /// on DB servers from 3.8 onwards.
  QueryId clusterQueryId =
      _query.vocbase().server().getFeature<ClusterFeature>().clusterInfo().uniqid();

  // decreases lock timeout manually for fast path
  auto oldLockTimeout = _query.getLockTimeout();
  _query.setLockTimeout(FAST_PATH_LOCK_TIMEOUT);
  std::mutex serverToQueryIdLock{};
  std::vector<std::tuple<ServerID, std::shared_ptr<VPackBuffer<uint8_t>>, std::vector<bool>>> engineInformation;
  engineInformation.reserve(dbServers.size());
  serverToQueryId.reserve(dbServers.size());

  for (ServerID const& server : dbServers) {
    // Build Lookup Infos
    VPackBuilder infoBuilder;
    auto didCreateEngine =
        buildEngineInfo(clusterQueryId, infoBuilder, server, nodesById, nodeAliases);
    VPackSlice infoSlice = infoBuilder.slice();

    if (isNotSatelliteLeader(infoSlice)) {
      continue;
    }

    if (!trx.state()->knownServers().contains(server)) {
      // we are about to add this server to the transaction.
      // remember it, so we can roll the addition back for
      // the second setup request if we need to
      serversAdded.emplace(server);
    }

    networkCalls.emplace_back(
        buildSetupRequest(trx, server, infoSlice, didCreateEngine, snippetIds,
                          serverToQueryId, serverToQueryIdLock, pool, options));
    engineInformation.emplace_back(server, infoBuilder.steal(), std::move(didCreateEngine));
    _query.incHttpRequests(unsigned(1));
  }

  futures::Future<Result> fastPathResult =
      futures::collectAll(networkCalls)
          .thenValue([](std::vector<arangodb::futures::Try<Result>>&& responses) -> Result {
            // We can directly report a non TRI_ERROR_LOCK_TIMEOUT
            // error as we need to abort after.
            // Otherwise we need to report
            Result res;
            for (auto const& tryRes : responses) {
              auto response = tryRes.get();
              if (response.fail()) {
                if (response.isNot(TRI_ERROR_LOCK_TIMEOUT)) {
                  // Found something we cannot recover from.
                  // Return and give up
                  return response;
                }
                // track that we have lock_timeout_present
                res = response;
              }
            }
            // Return what we have, this will be ok() if and only
            // if none of the requests failed.
            // It will be LOCK_TIMEOUT if and only if the only error
            // we see was LOCK_TIMEOUT.
            return res;
          });
  if (fastPathResult.get().fail()) {
    if (fastPathResult.get().isNot(TRI_ERROR_LOCK_TIMEOUT)) {
      // we got an error. this will trigger the cleanupGuard!
      // set the proper error reason.
      cleanupReason = fastPathResult.get().errorNumber();
      return fastPathResult.get();
    }

    // we got a lock timeout response for the fast path locking...
    {
      // in case of fast path failure, we need to cleanup engines
      auto requests = cleanupEngines(fastPathResult.get().errorNumber(),
                                     _query.vocbase().name(), serverToQueryId);
      // Wait for all cleanup requests to complete.
      // So we know that all Transactions are aborted.
      Result res;
      for (auto& tryRes : requests) {
        network::Response const& response = tryRes.get();
        if (response.fail()) {
          // note first error, but continue iterating over all results
          LOG_TOPIC("2d319", DEBUG, Logger::AQL)
              << "received error from server " << response.destination
              << " during query cleanup: " << response.combinedResult().errorMessage();
          res.reset(response.combinedResult());
        }
      }
      if (res.fail()) {
        // unable to do a proper cleanup.
        // it is not safe to go on here.
        cleanupGuard.cancel();
        cleanupReason = res.errorNumber();
        return res;
      }
    }

    snippetIds.clear();

    // revert the addition of servers by us
    for (auto const& s : serversAdded) {
      trx.state()->removeKnownServer(s);
    }

    // fast path locking rolled back successfully!
    TRI_ASSERT(serverToQueryId.empty());

    // we must generate a new query id, because the fast path setup has failed
    clusterQueryId =
        _query.vocbase().server().getFeature<ClusterFeature>().clusterInfo().uniqid();

    if (trx.isMainTransaction() && !trx.state()->isReadOnlyTransaction()) {
      // when we are not in a streaming transaction, it is ok to roll a new trx
      // id. it is not ok to change the trx id inside a streaming transaction,
      // because then the caller would not be able to "talk" to the transaction
      // any further.
      // note: read-only transactions do not need to reroll their id, as there
      // will be no locks taken.
      trx.state()->coordinatorRerollTransactionId();
    }

    // set back to default lock timeout for slow path fallback
    _query.setLockTimeout(oldLockTimeout);
    LOG_TOPIC("f5022", DEBUG, Logger::AQL)
        << "Potential deadlock detected, using slow path for locking. This "
           "is expected if exclusive locks are used.";

    // Make sure we always use the same ordering on servers
    std::sort(engineInformation.begin(), engineInformation.end(),
              [](auto const& lhs, auto const& rhs) {
                // Entry <0> is the Server
                return TransactionState::ServerIdLessThan(std::get<0>(lhs),
                                                          std::get<0>(rhs));
              });
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // Make sure we always maintain the correct ordering of servers
    // here, if we contact them in increasing name, we avoid dead-locks
    std::string serverBefore = "";
#endif
    // fallback routine, use synchronous requests (slowPath)
    for (auto& [server, buffer, didCreateEngine] : engineInformation) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // If the serverBefore has a smaller ID we allways contact by increasing
      // ID here.
      TRI_ASSERT(TransactionState::ServerIdLessThan(serverBefore, server));
      serverBefore = server;
#endif
      VPackSlice infoSlice{buffer->data()};
      // We need to rewrite the request.
      // We have modified the options locally so we need to update the VPack representation of Options here.
      // Note: There may be a more optimized variant, but we have just waited 2 seconds and will linearly
      // lock all servers, any performance optimization here will not have measureable impact.
      VPackBuilder overwrittenOptions;
      overwrittenOptions.openObject();
      // patch query id
      overwrittenOptions.add("clusterQueryId", VPackValue(clusterQueryId));
      addOptionsPart(overwrittenOptions, server);
      overwrittenOptions.close();
      auto newRequest =
          arangodb::velocypack::Collection::merge(infoSlice,
                                                  overwrittenOptions.slice(), false);

      auto request = buildSetupRequest(trx, std::move(server), newRequest.slice(),
                                       std::move(didCreateEngine), snippetIds, serverToQueryId,
                                       serverToQueryIdLock, pool, options);
      _query.incHttpRequests(unsigned(1));
      if (request.get().fail()) {
        // this will trigger the cleanupGuard.
        // set the proper error reason
        cleanupReason = request.get().errorNumber();
        return request.get();
      }
    }
  }

  cleanupGuard.cancel();
  return {};
}

Result EngineInfoContainerDBServerServerBased::parseResponse(
    VPackSlice response, MapRemoteToSnippet& queryIds, ServerID const& server,
    std::vector<bool> const& didCreateEngine, QueryId& globalQueryId,
    RebootId& rebootId) const {
  TRI_ASSERT(server.substr(0, 7) != "server:");

  if (!response.isObject() || !response.get("result").isObject()) {
    LOG_TOPIC("0c3f2", WARN, Logger::AQL) << "Received error information from "
                                          << server << ": " << response.toJson();
    if (response.hasKey(StaticStrings::ErrorNum) &&
        response.hasKey(StaticStrings::ErrorMessage)) {
      return network::resultFromBody(response, TRI_ERROR_CLUSTER_AQL_COMMUNICATION)
          .withError([&](result::Error& err) {
            err.appendErrorMessage(StringUtils::concatT(". Please check: ", server));
          });
    }
    return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
            "Unable to deploy query on all required "
            "servers: " +
                response.toJson() +
                ". This can happen during "
                "failover. Please check: " +
                server};
  }

  VPackSlice result = response.get("result");
  VPackSlice queryIdSlice = result.get("queryId");

  if (queryIdSlice.isNumber()) {
    // populate globalQueryId only if present in response (3.7 and before).
    // 3.8 DB servers will not populate this attribute in their responses!
    // this is fine (tm), because then the coordinator will assume that all
    // DB servers will have used the global query ID that the coordinator
    // had prescribed
    globalQueryId = queryIdSlice.getNumber<QueryId>();
  }

  VPackSlice rebootIdSlice = result.get(StaticStrings::RebootId);
  if (rebootIdSlice.isNumber()) {
    rebootId = RebootId(rebootIdSlice.getNumber<uint64_t>());
  }

  VPackSlice snippets = result.get("snippets");
  // Link Snippets to their sinks
  for (auto const& resEntry : VPackObjectIterator(snippets)) {
    if (!resEntry.value.isString()) {
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "Unable to deploy query snippets on all required "
              "servers. This can happen during "
              "failover. Please check: " +
                  server};
    }
    auto remoteId = ExecutionNodeId{0};
    std::string shardId;
    auto res = ExtractRemoteAndShard(resEntry.key, remoteId, shardId);
    if (!res.ok()) {
      return res;
    }
    TRI_ASSERT(remoteId != ExecutionNodeId{0});
    TRI_ASSERT(!shardId.empty());
    auto& remote = queryIds[remoteId];
    auto& thisServer = remote["server:" + server];
    thisServer.emplace_back(resEntry.value.copyString());
  }

  // Link traverser engines to their nodes
  VPackSlice travEngines = result.get("traverserEngines");
  if (!travEngines.isNone()) {
    if (!travEngines.isArray()) {
      return {TRI_ERROR_CLUSTER_AQL_COMMUNICATION,
              "Unable to deploy query traverser engines on all required "
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
        _graphNodes[i]->addEngine(idIter.value().getNumber<aql::EngineId>(), server);
        idIter.next();
      }
    }
    // We need to consume all traverser engines
    TRI_ASSERT(!idIter.valid());
  }
  return {};
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
std::vector<arangodb::network::FutureRes> EngineInfoContainerDBServerServerBased::cleanupEngines(
    ErrorCode errorCode, std::string const& dbname,
    aql::ServerQueryIdList& serverQueryIds) const {
  std::vector<arangodb::network::FutureRes> requests;
  NetworkFeature const& nf = _query.vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();

  network::RequestOptions options;
  options.database = dbname;
  options.timeout = network::Timeout(10.0);  // Picked arbitrarily
  options.skipScheduler = true;              // hack to speed up future.get()

  // Shutdown query snippets
  VPackBuffer<uint8_t> body;
  VPackBuilder builder(body);
  builder.openObject();
  builder.add(StaticStrings::Code, VPackValue(errorCode));
  builder.close();
  requests.reserve(serverQueryIds.size());
  for (auto const& [server, queryId, rebootId] : serverQueryIds) {
    TRI_ASSERT(server.substr(0, 7) != "server:");
    requests.emplace_back(
        network::sendRequestRetry(pool, "server:" + server, fuerte::RestVerb::Delete,
                                  ::finishUrl + std::to_string(queryId),
                                  /*copy*/ body, options));
  }
  _query.incHttpRequests(static_cast<unsigned>(serverQueryIds.size()));

  // Shutdown traverser engines
  VPackBuffer<uint8_t> noBody;

  for (auto& gn : _graphNodes) {
    auto allEngines = gn->engines();
    for (auto const& engine : *allEngines) {
      TRI_ASSERT(engine.first.substr(0, 7) != "server:");
      requests.emplace_back(network::sendRequestRetry(
          pool, "server:" + engine.first, fuerte::RestVerb::Delete,
          ::traverserUrl + basics::StringUtils::itoa(engine.second), noBody, options));
    }
    _query.incHttpRequests(static_cast<unsigned>(allEngines->size()));
    gn->clearEngines();
  }

  serverQueryIds.clear();
  return requests;
}

// Insert a GraphNode that needs to generate TraverserEngines on
// the DBServers. The GraphNode itself will retain on the coordinator.
void EngineInfoContainerDBServerServerBased::addGraphNode(GraphNode* node, bool pushToSingleServer) {
  node->prepareOptions();
  injectVertexCollections(node);
  node->initializeIndexConditions();
  // SnippetID does not matter on GraphNodes
  _shardLocking.addNode(node, 0, pushToSingleServer);
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
  if (_query.queryOptions().transactionOptions.skipInaccessibleCollections) {
    aql::QueryOptions opts = _query.queryOptions();
    TRI_ASSERT(opts.transactionOptions.skipInaccessibleCollections);
    std::vector<Collection const*> used = _shardLocking.getUsedCollections();
    for (Collection const* coll : used) {
      TRI_ASSERT(coll != nullptr);
      // simon: add collection name, plan ID and shard IDs
      if (_query.trxForOptimization().isInaccessibleCollection(coll->id())) {
        for (ShardID const& sid : _shardLocking.getShardsForCollection(server, coll)) {
          opts.inaccessibleCollections.insert(sid);
        }
        opts.inaccessibleCollections.insert(std::to_string(coll->id().id()));
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

// Insert the Variables information into the message to be sent to DBServers
void EngineInfoContainerDBServerServerBased::addVariablesPart(arangodb::velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("variables"));
  // This will open and close an Object.
  _query.ast()->variables()->toVelocyPack(builder);
}

// Insert the Snippets information into the message to be sent to DBServers
void EngineInfoContainerDBServerServerBased::addSnippetPart(
    std::unordered_map<ExecutionNodeId, ExecutionNode*> const& nodesById,
    arangodb::velocypack::Builder& builder, ShardLocking& shardLocking,
    std::map<ExecutionNodeId, ExecutionNodeId>& nodeAliases, ServerID const& server) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add(VPackValue("snippets"));
  builder.openObject();
  for (auto const& snippet : _closedSnippets) {
    snippet->serializeIntoBuilder(server, nodesById, shardLocking, nodeAliases, builder);
  }
  builder.close();  // snippets
}

// Insert the TraversalEngine information into the message to be sent to DBServers
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
