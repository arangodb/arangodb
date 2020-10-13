////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterInfo.h"
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
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Buffer.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::futures;

namespace {

void buildTransactionBody(TransactionState& state, ServerID const& server,
                          VPackBuilder& builder) {
  // std::vector<ServerID> DBservers = ci->getCurrentDBServers();
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

/// @brief lazy begin a transaction on subordinate servers
Future<network::Response> beginTransactionRequest(TransactionState& state,
                                                  ServerID const& server) {
  TransactionId tid = state.id().child();
  TRI_ASSERT(!tid.isLegacyTransactionId());

  VPackBuffer<uint8_t> buffer;
  VPackBuilder builder(buffer);
  buildTransactionBody(state, server, builder);
  
  network::RequestOptions reqOpts;
  reqOpts.database = state.vocbase().name();

  auto* pool = state.vocbase().server().getFeature<NetworkFeature>().pool();
  network::Headers headers;
  headers.try_emplace(StaticStrings::TransactionId, std::to_string(tid.id()));
  auto body = std::make_shared<std::string>(builder.slice().toJson());
  return network::sendRequest(pool, "server:" + server, fuerte::RestVerb::Post,
                              "/_api/transaction/begin", std::move(buffer), reqOpts, std::move(headers));
}

/// check the transaction cluster response with desited TID and status
Result checkTransactionResult(TransactionId desiredTid, transaction::Status desStatus,
                              network::Response const& resp) {
  int commError = network::fuerteToArangoErrorCode(resp);
  if (commError != TRI_ERROR_NO_ERROR) {
    // oh-oh cluster is in a bad state
    return Result(commError);
  }

  VPackSlice answer = resp.response->slice();
  if ((resp.response->statusCode() == fuerte::StatusOK ||
       resp.response->statusCode() == fuerte::StatusCreated) &&
      answer.isObject()) {
    VPackSlice idSlice = answer.get(std::vector<std::string>{"result", "id"});
    VPackSlice statusSlice =
        answer.get(std::vector<std::string>{"result", "status"});

    if (!idSlice.isString() || !statusSlice.isString()) {
      return Result(TRI_ERROR_TRANSACTION_INTERNAL,
                    "transaction has wrong format");
    }
    TransactionId tid{StringUtils::uint64(idSlice.copyString())};
    VPackValueLength len = 0;
    const char* str = statusSlice.getStringUnchecked(len);
    if (tid == desiredTid && transaction::statusFromString(str, len) == desStatus) {
      return Result();  // success
    }
  } else if (answer.isObject()) {
    std::string msg = std::string(" (error while ");
    if (desStatus == transaction::Status::RUNNING) {
      msg.append("beginning transaction)");
    } else if (desStatus == transaction::Status::COMMITTED) {
      msg.append("committing transaction)");
    } else if (desStatus == transaction::Status::ABORTED) {
      msg.append("aborting transaction)");
    }
    Result res = network::resultFromBody(answer, TRI_ERROR_TRANSACTION_INTERNAL);
    res.appendErrorMessage(msg);
    return res;
  }
  LOG_TOPIC("fb343", DEBUG, Logger::TRANSACTIONS)
      << "failed to begin transaction on " << resp.destination;

  return Result(TRI_ERROR_TRANSACTION_INTERNAL);  // unspecified error
}

Future<Result> commitAbortTransaction(transaction::Methods& trx, transaction::Status status) {
  arangodb::TransactionState* state = trx.state();
  TRI_ASSERT(state->isRunning());
  TRI_ASSERT(trx.isMainTransaction());

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

  TransactionId tidPlus = state->id().child();
  const std::string path = "/_api/transaction/" + std::to_string(tidPlus.id());

  fuerte::RestVerb verb;
  if (status == transaction::Status::COMMITTED) {
    verb = fuerte::RestVerb::Put;
  } else if (status == transaction::Status::ABORTED) {
    verb = fuerte::RestVerb::Delete;
  } else {
    TRI_ASSERT(false);
  }

  auto* pool = trx.vocbase().server().getFeature<NetworkFeature>().pool();
  std::vector<Future<network::Response>> requests;
  requests.reserve(state->knownServers().size());
  for (std::string const& server : state->knownServers()) {
    requests.emplace_back(network::sendRequest(pool, "server:" + server, verb,
                                               path, VPackBuffer<uint8_t>(), reqOpts));
  }

  return futures::collectAll(requests).thenValue(
      [=](std::vector<Try<network::Response>>&& responses) -> Result {
        if (state->isCoordinator()) {
          TRI_ASSERT(state->id().isCoordinatorTransactionId());

          for (Try<arangodb::network::Response> const& tryRes : responses) {
            network::Response const& resp = tryRes.get();  // throws exceptions upwards
            Result res = ::checkTransactionResult(tidPlus, status, resp);
            if (res.fail()) {
              return res;
            }
          }

          return Result();
        }

        TRI_ASSERT(state->isDBServer());
        TRI_ASSERT(state->id().isLeaderTransactionId());

        // Drop all followers that were not successful:
        for (Try<arangodb::network::Response> const& tryRes : responses) {
          network::Response const& resp = tryRes.get();  // throws exceptions upwards

          Result res = ::checkTransactionResult(tidPlus, status, resp);
          if (res.fail()) {  // remove follower from all collections
            ServerID follower = resp.serverId();
            LOG_TOPIC("230c3", INFO, Logger::REPLICATION) 
                << "synchronous replication: dropping follower " 
                << follower << " for all participating shards in"
                << " transaction " << state->id().id() << " (status " 
                << arangodb::transaction::statusString(status) 
                << "), status code: " << static_cast<int>(resp.statusCode()) 
                << ", message: " << resp.combinedResult().errorMessage();
            state->allCollections([&](TransactionCollection& tc) {
              auto cc = tc.collection();
              if (cc) {
                LOG_TOPIC("709c9", WARN, Logger::REPLICATION)
                    << "synchronous replication: dropping follower "
                    << follower << " for shard " << tc.collectionName()
                    << " in database " << cc->vocbase().name() 
                    << ": " << resp.combinedResult().errorMessage();

                Result r = cc->followers()->remove(follower);
                if (r.fail()) {
                  LOG_TOPIC("4971f", ERR, Logger::REPLICATION)
                      << "synchronous replication: could not drop follower "
                      << follower << " for shard " << tc.collectionName()
                      << ": " << r.errorMessage();
                  res.reset(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
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
}  // namespace

namespace arangodb {
namespace ClusterTrxMethods {
using namespace arangodb::futures;

/// @brief begin a transaction on all leaders
Future<Result> beginTransactionOnLeaders(TransactionState& state,
                                         std::vector<ServerID> const& leaders) {
  TRI_ASSERT(state.isCoordinator());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
  Result res;
  if (leaders.empty()) {
    return res;
  }

  std::vector<Future<network::Response>> requests;
  for (ServerID const& leader : leaders) {
    if (state.knowsServer(leader)) {
      continue;  // already send a begin transaction there
    }
    state.addKnownServer(leader);
    requests.emplace_back(::beginTransactionRequest(state, leader));
  }

  if (requests.empty()) {
    return res;
  }

  const TransactionId tid = state.id().child();
  return futures::collectAll(requests).thenValue(
      [=](std::vector<Try<network::Response>>&& responses) -> Result {
        for (Try<arangodb::network::Response> const& tryRes : responses) {
          network::Response const& resp = tryRes.get();  // throws exceptions upwards
          Result res = ::checkTransactionResult(tid, transaction::Status::RUNNING, resp);
          if (res.fail()) {  // remove follower from all collections
            return res;
          }
        }
        return Result();  // all good
      });
}

/// @brief commit a transaction on a subordinate
Future<Result> commitTransaction(transaction::Methods& trx) {
  return commitAbortTransaction(trx, transaction::Status::COMMITTED);
}

/// @brief commit a transaction on a subordinate
Future<Result> abortTransaction(transaction::Methods& trx) {
  return commitAbortTransaction(trx, transaction::Status::ABORTED);
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

  const bool addBegin = !state.knowsServer(server);
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

  std::string value = std::to_string(state.id().child().id());
  const bool addBegin = !state.knowsServer(server);
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
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "illegal AQL transaction state");
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

}  // namespace ClusterTrxMethods
}  // namespace arangodb
