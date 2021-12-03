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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "ClusterTrxMethods.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
#include "Futures/Utilities.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/MethodsApi.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;

namespace {
// Wait 2s to get the Lock in FastPath, otherwise assume dead-lock.
const double FAST_PATH_LOCK_TIMEOUT = 2.0;

void buildTransactionBody(TransactionState& state, ServerID const& server,
                          VPackBuilder& builder) {
  builder.openObject();
  state.options().toVelocyPack(builder);
  builder.add("collections", VPackValue(VPackValueType::Object));
  auto addCollections = [&](const char* key, AccessMode::Type t) {
    size_t numCollections = 0;
    state.allCollections([&](TransactionCollection& col) {
      if (col.accessType() != t) {
        return true;
      }
      if (!state.isCoordinator()) {
        TRI_IF_FAILURE("buildTransactionBodyEmpty") {
          return true;  // continue
        }

        if (col.collection()->followers()->contains(server)) {
          if (numCollections == 0) {
            builder.add(key, VPackValue(VPackValueType::Array));
          }
          builder.add(VPackValue(col.collectionName()));
          numCollections++;
        }
        return true;  // continue
      }

      // coordinator starts transaction on shard leaders
#ifdef USE_ENTERPRISE
      if (col.collection()->isSmart() && col.collection()->type() == TRI_COL_TYPE_EDGE) {
        auto names = col.collection()->realNames();
        auto& ci =
            col.collection()->vocbase().server().getFeature<ClusterFeature>().clusterInfo();
        for (std::string const& name : names) {
          auto cc = ci.getCollectionNT(state.vocbase().name(), name);
          if (!cc) {
            continue;
          }
          auto shards = ci.getShardList(std::to_string(cc->id().id()));
          for (ShardID const& shard : *shards) {
            auto sss = ci.getResponsibleServer(shard);
            if (server == sss->at(0)) {
              if (numCollections == 0) {
                builder.add(key, VPackValue(VPackValueType::Array));
              }
              builder.add(VPackValue(shard));
              numCollections++;
            }
          }
        }
        return true;  // continue
      }
#endif
      std::shared_ptr<ShardMap> shardIds = col.collection()->shardIds();
      for (auto const& pair : *shardIds) {
        TRI_ASSERT(!pair.second.empty());
        // only add shard where server is leader
        if (!pair.second.empty() && pair.second[0] == server) {
          if (numCollections == 0) {
            builder.add(key, VPackValue(VPackValueType::Array));
          }
          builder.add(VPackValue(pair.first));
          numCollections++;
        }
      }
      return true;
    });
    if (numCollections != 0) {
      builder.close();
    }
  };
  addCollections("read", AccessMode::Type::READ);
  addCollections("write", AccessMode::Type::WRITE);
  addCollections("exclusive", AccessMode::Type::EXCLUSIVE);

  builder.close();  // </collections>
  builder.close();  // </openObject>
}

/// @brief lazily begin a transaction on subordinate servers
Future<network::Response> beginTransactionRequest(TransactionState& state,
                                                  ServerID const& server,
                                                  transaction::MethodsApi api) {
  TransactionId tid = state.id().child();
  TRI_ASSERT(!tid.isLegacyTransactionId());
  
  TRI_ASSERT(server.substr(0, 7) != "server:");

  double const lockTimeout = state.options().lockTimeout;

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  buildTransactionBody(state, server, builder);

  network::RequestOptions reqOpts;
  reqOpts.database = state.vocbase().name();
  // set request timeout a little higher than our lock timeout, so that 
  // responses that are close to the timeout value have a chance of getting
  // back to us (note: the 5 is arbitrary here, could as well be 3.0 or 10.0)
  reqOpts.timeout = network::Timeout(lockTimeout + 5.0);
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;

  auto* pool = state.vocbase().server().getFeature<NetworkFeature>().pool();
  network::Headers headers;
  headers.try_emplace(StaticStrings::TransactionId, std::to_string(tid.id()));
  auto body = std::make_shared<std::string>(builder.slice().toJson());
  return network::sendRequestRetry(pool, "server:" + server, fuerte::RestVerb::Post,
                              "/_api/transaction/begin", std::move(buffer),
                              reqOpts, std::move(headers));
}

/// check the transaction cluster response with desited TID and status
Result checkTransactionResult(TransactionId desiredTid, transaction::Status desStatus,
                              network::Response const& resp) {
  Result r = resp.combinedResult();

  if (resp.fail()) {
    // communication error
    return r;
  }

  // whatever we got can contain a success (HTTP 2xx) or an error (HTTP >= 400) 
  if (VPackSlice answer = resp.slice(); (resp.statusCode() == fuerte::StatusOK || resp.statusCode() == fuerte::StatusCreated) &&
      answer.isObject()) {
    VPackSlice idSlice = answer.get({"result", "id"});
    VPackSlice statusSlice = answer.get({"result", "status"});


    if (!idSlice.isString() || !statusSlice.isString()) {
      return r.reset(TRI_ERROR_TRANSACTION_INTERNAL, "transaction has wrong format");
    }

    std::string_view idRef = idSlice.stringView();
    TransactionId tid{StringUtils::uint64(idRef.data(), idRef.size())};
    std::string_view statusRef = statusSlice.stringView();
    if (tid == desiredTid && transaction::statusFromString(statusRef.data(), statusRef.size()) == desStatus) {
      // all good
      return r.reset();
    }
  }
  
  if (!r.fail()) {
    r.reset(TRI_ERROR_TRANSACTION_INTERNAL);
  }
    
  TRI_ASSERT(r.fail());
  std::string msg(" (error while ");
  if (desStatus == transaction::Status::RUNNING) {
    msg.append("beginning transaction on ");
  } else if (desStatus == transaction::Status::COMMITTED) {
    msg.append("committing transaction on ");
  } else if (desStatus == transaction::Status::ABORTED) {
    msg.append("aborting transaction on ");
  }
  msg.append(resp.destination);
  msg.append(")");

  return r.withError([&](result::Error& err) { err.appendErrorMessage(msg); });
}

Future<Result> commitAbortTransaction(arangodb::TransactionState* state,
                                      transaction::Status status,
                                      transaction::MethodsApi api) {
  TRI_ASSERT(state->isRunning());

  if (state->knownServers().empty()) {
    return Result();
  }

  // only commit managed transactions, and AQL leader transactions (on DBServers)
  if (!ClusterTrxMethods::isElCheapo(*state) ||
      (state->isCoordinator() && state->hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL))) {
    return Result();
  }
  TRI_ASSERT(!state->isDBServer() || !state->id().isFollowerTransactionId());

  network::RequestOptions reqOpts;
  reqOpts.database = state->vocbase().name();
  reqOpts.skipScheduler = api == transaction::MethodsApi::Synchronous;

  TransactionId tidPlus = state->id().child();
  std::string const path = "/_api/transaction/" + std::to_string(tidPlus.id());
  if (state->isDBServer()) {
    // This is a leader replicating the transaction commit or abort and
    // we should tell the follower that this is a replication operation.
    // It will then execute the request with a higher priority.
    reqOpts.param(StaticStrings::IsSynchronousReplicationString,
                  ServerState::instance()->getId());
  }

  char const* stateString = nullptr;
  fuerte::RestVerb verb;
  if (status == transaction::Status::COMMITTED) {
    stateString = "commit";
    verb = fuerte::RestVerb::Put;
  } else if (status == transaction::Status::ABORTED) {
    stateString = "abort";
    verb = fuerte::RestVerb::Delete;
  } else {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "invalid state for commit/abort operation");
  }

  auto* pool = state->vocbase().server().getFeature<NetworkFeature>().pool();
  std::vector<Future<network::Response>> requests;
  requests.reserve(state->knownServers().size());
  for (std::string const& server : state->knownServers()) {
    TRI_ASSERT(server.substr(0, 7) != "server:");
    requests.emplace_back(network::sendRequestRetry(pool, "server:" + server, verb, path,
                                               VPackBuffer<uint8_t>(), reqOpts));
  }

  return futures::collectAll(requests).thenValue(
      [=](std::vector<Try<network::Response>>&& responses) -> Result {
        if (state->isCoordinator()) {
          TRI_ASSERT(state->id().isCoordinatorTransactionId());

          Result res;
          for (Try<arangodb::network::Response> const& tryRes : responses) {
            network::Response const& resp = tryRes.get();  // throws exceptions upwards
            res = ::checkTransactionResult(tidPlus, status, resp);
            if (res.fail()) {
              break;
            }
          }

          return res;
        }

        TRI_ASSERT(state->isDBServer());
        TRI_ASSERT(state->id().isLeaderTransactionId());

        // Drop all followers that were not successful:
        for (Try<arangodb::network::Response> const& tryRes : responses) {
          network::Response const& resp = tryRes.get();  // throws exceptions upwards

          Result res = ::checkTransactionResult(tidPlus, status, resp);
          if (res.fail()) {  // remove followers for all participating collections
            ServerID follower = resp.serverId();
            LOG_TOPIC("230c3", INFO, Logger::REPLICATION)
                << "synchronous replication of transaction " << stateString << " operation: "
                << "dropping follower " << follower << " for all participating shards in"
                << " transaction " << state->id().id() << " (status "
                << arangodb::transaction::statusString(status)
                << "), status code: " << static_cast<int>(resp.statusCode())
                << ", message: " << resp.combinedResult().errorMessage();
            state->allCollections([&](TransactionCollection& tc) {
              auto cc = tc.collection();
              if (cc) {
                LOG_TOPIC("709c9", WARN, Logger::REPLICATION)
                    << "synchronous replication of transaction " << stateString << " operation: "
                    << "dropping follower " << follower << " for shard " 
                    << cc->vocbase().name() << "/" << tc.collectionName() << ": "
                    << resp.combinedResult().errorMessage();

                Result r = cc->followers()->remove(follower);
                if (r.fail()) {
                  LOG_TOPIC("4971f", ERR, Logger::REPLICATION)
                      << "synchronous replication: could not drop follower " << follower
                      << " for shard " << cc->vocbase().name() << "/" << tc.collectionName()
                      << ": " << r.errorMessage();
                  if (res.is(TRI_ERROR_CLUSTER_NOT_LEADER)) {
                    // In this case, we know that we are not or no longer
                    // the leader for this shard. Therefore we need to
                    // send a code which let's the coordinator retry.
                    res.reset(TRI_ERROR_CLUSTER_SHARD_LEADER_RESIGNED);
                  } else {
                    // In this case, some other error occurred and we
                    // most likely are still the proper leader, so
                    // the error needs to be reported and the local
                    // transaction must be rolled back.
                    res.reset(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
                  }
                }
              }
              // continue dropping the follower for all shards in this
              // transaction
              return true;
            });
          }
        }
        return Result();  // succeed even if some followers did not commit
      });
}

Future<Result> commitAbortTransaction(transaction::Methods& trx, transaction::Status status,
                                      transaction::MethodsApi api) {
  arangodb::TransactionState* state = trx.state();
  TRI_ASSERT(trx.isMainTransaction());
  return commitAbortTransaction(state, status, api);
}

}  // namespace

namespace arangodb::ClusterTrxMethods {
using namespace arangodb::futures;

bool IsServerIdLessThan::operator()(ServerID const& lhs, ServerID const& rhs) const noexcept {
  return TransactionState::ServerIdLessThan(lhs, rhs);
}

/// @brief begin a transaction on all leaders
Future<Result> beginTransactionOnLeaders(TransactionState& state, ClusterTrxMethods::SortedServersSet const& leaders,
    // everything in this function is done synchronously, so the `api` parameter is currently unused.
    [[maybe_unused]] transaction::MethodsApi api) {
  TRI_ASSERT(state.isCoordinator());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
  Result res;
  if (leaders.empty()) {
    return res;
  }

  // If !state.knownServers.empty() => We have already locked something.
  //   We cannot revert fastPath locking and continue over slowpath. (Trx may be used)
  bool canRevertToSlowPath =
      state.hasHint(transaction::Hints::Hint::ALLOW_FAST_LOCK_ROUND_CLUSTER) &&
      state.knownServers().empty();

  double oldLockTimeout = state.options().lockTimeout;
  {
    if (canRevertToSlowPath) {
      // We first try to do a fast lock, if we cannot get this
      // There is a potential dead lock situation
      // and we revert to a slow locking to be on the safe side.
      state.options().lockTimeout = FAST_PATH_LOCK_TIMEOUT;
    }
    // Run fastPath
    std::vector<Future<network::Response>> requests;
    for (ServerID const& leader : leaders) {
      if (state.knowsServer(leader)) {
        continue;  // already sent a begin transaction there
      }
      requests.emplace_back(
          ::beginTransactionRequest(state, leader, transaction::MethodsApi::Synchronous));
    }

    // use original lock timeout here
    state.options().lockTimeout = oldLockTimeout;

    if (requests.empty()) {
      return res;
    }

    const TransactionId tid = state.id().child();

    Result fastPathResult =
        futures::collectAll(requests)
            .thenValue([&tid, &state](std::vector<Try<network::Response>>&& responses) -> Result {
              // We need to make sure to get() all responses.
              // Otherwise they will eventually resolve and trigger the .then() callback
              // which might be after we left this function.
              // Especially if one response errors with "non-repairable" code so
              // we actually abort here and cannot revert to slow path execution.
              Result result{TRI_ERROR_NO_ERROR};
              for (Try<arangodb::network::Response> const& tryRes : responses) {
                network::Response const& resp = tryRes.get();  // throws exceptions upwards

                Result res =
                    ::checkTransactionResult(tid, transaction::Status::RUNNING, resp);
                if (res.fail()) {
                  if (!result.fail() || result.is(TRI_ERROR_LOCK_TIMEOUT)) {
                    result = res;
                  }
                } else {
                  state.addKnownServer(resp.serverId());  // add server id to known list
                }
              }

              return result;
            })
            .get();

    if (fastPathResult.isNot(TRI_ERROR_LOCK_TIMEOUT) || !canRevertToSlowPath) {
      // We are either good or we cannot use the slow path.
      // We need to return the result here.
      // We made sure that all servers that reported success are known to the transaction.
      return fastPathResult;
    }

    // Entering slow path

    TRI_ASSERT(fastPathResult.is(TRI_ERROR_LOCK_TIMEOUT));

    // abortTransaction on knownServers() and wait for them
    if (!state.knownServers().empty()) {
      Result resetRes = commitAbortTransaction(&state, transaction::Status::ABORTED,
                                               transaction::MethodsApi::Synchronous)
                            .get();
      if (resetRes.fail()) {
        // return here if cleanup failed - this needs to be a success
        return resetRes;
      }
    }

    // rerollTrxId() - this also clears _knownServers (!)
    state.coordinatorRerollTransactionId();

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    // Make sure we always maintain the correct ordering of servers
    // here, if we contact them in increasing name, we avoid dead-locks
    std::string serverBefore = "";
#endif
    // Run slowPath
    for (ServerID const& leader : leaders) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      // If the serverBefore has a smaller ID we allways contact by increasing
      // ID here.
      TRI_ASSERT(TransactionState::ServerIdLessThan(serverBefore, leader));
      serverBefore = leader;
#endif

      auto resp = ::beginTransactionRequest(state, leader, transaction::MethodsApi::Synchronous);
      auto const& resolvedResponse = resp.get();
      if (resolvedResponse.fail()) {
        return resolvedResponse.combinedResult();
      } else {
        state.addKnownServer(leader);  // add server id to known list
      }
    }
  }

  return TRI_ERROR_NO_ERROR;
}

/// @brief commit a transaction on a subordinate
Future<Result> commitTransaction(transaction::Methods& trx, transaction::MethodsApi api) {
  return commitAbortTransaction(trx, transaction::Status::COMMITTED, api);
}

/// @brief commit a transaction on a subordinate
Future<Result> abortTransaction(transaction::Methods& trx, transaction::MethodsApi api) {
  return commitAbortTransaction(trx, transaction::Status::ABORTED, api);
}

/// @brief set the transaction ID header
template <typename MapT>
void addTransactionHeader(transaction::Methods const& trx,
                          ServerID const& server, MapT& headers) {
  TransactionState& state = *trx.state();
  TRI_ASSERT(state.isRunningInCluster());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;  // no need
  }
  TransactionId tidPlus = state.id().child();
  TRI_ASSERT(!tidPlus.isLegacyTransactionId());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));

  bool const addBegin = !state.knowsServer(server);
  if (addBegin) {
    if (state.isCoordinator() && state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
      return;  // do not add header to servers without a snippet
    }
    TRI_ASSERT(state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED) ||
               state.id().isLeaderTransactionId());
    transaction::BuilderLeaser builder(trx.transactionContextPtr());
    ::buildTransactionBody(state, server, *builder.get());
    headers.try_emplace(StaticStrings::TransactionBody, builder->toJson());
    headers.try_emplace(arangodb::StaticStrings::TransactionId,
                        std::to_string(tidPlus.id()).append(" begin"));
    state.addKnownServer(server);  // remember server
  } else {
    headers.try_emplace(arangodb::StaticStrings::TransactionId,
                        std::to_string(tidPlus.id()));
  }
}
template void addTransactionHeader<std::map<std::string, std::string>>(
    transaction::Methods const&, ServerID const&, std::map<std::string, std::string>&);
template void addTransactionHeader<std::unordered_map<std::string, std::string>>(
    transaction::Methods const&, ServerID const&,
    std::unordered_map<std::string, std::string>&);

/// @brief add transaction ID header for setting up AQL snippets
template <typename MapT>
void addAQLTransactionHeader(transaction::Methods const& trx,
                             ServerID const& server, MapT& headers) {
  TransactionState& state = *trx.state();
  TRI_ASSERT(state.isCoordinator());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;
  }
  
  TRI_ASSERT(server.substr(0, 7) != "server:");

  std::string value = std::to_string(state.id().child().id());
  bool const addBegin = !state.knowsServer(server);
  if (addBegin) {
    if (state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
      value.append(" aql");  // This is a single AQL query
    } else if (state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
      transaction::BuilderLeaser builder(trx.transactionContextPtr());
      ::buildTransactionBody(state, server, *builder.get());
      headers.try_emplace(StaticStrings::TransactionBody, builder->toJson());
      value.append(" begin");  // part of a managed transaction
    } else {
      TRI_ASSERT(false);
    }
    state.addKnownServer(server);  // remember server
  } else if (state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
    // this case cannot occur for a top-level AQL query,
    // simon: however it might occur when UDF functions uses
    // db._query(...) in which case we can get here
    bool canHaveUDF = trx.transactionContext()->isV8Context();
    TRI_ASSERT(canHaveUDF);
    if (!canHaveUDF) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "illegal AQL transaction state");
    }
  }
  headers.try_emplace(arangodb::StaticStrings::TransactionId, std::move(value));
}
template void addAQLTransactionHeader<std::map<std::string, std::string>>(
    transaction::Methods const&, ServerID const&, std::map<std::string, std::string>&);

bool isElCheapo(transaction::Methods const& trx) {
  return isElCheapo(*trx.state());
}

bool isElCheapo(TransactionState const& state) {
  return !state.id().isLegacyTransactionId() &&
         (state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED) ||
          state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL));
}

}  // namespace arangodb::ClusterTrxMethods
