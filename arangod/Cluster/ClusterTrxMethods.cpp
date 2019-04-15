////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/NumberUtils.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterComm.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/FollowerInfo.h"
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

namespace {

// Timeout for read operations:
static double const CL_DEFAULT_TIMEOUT = 120.0;

void buildTransactionBody(TransactionState& state, ServerID const& server,
                          VPackBuilder& builder) {
  // std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  builder.openObject();
  state.options().toVelocyPack(builder);
  builder.add("collections", VPackValue(VPackValueType::Object));
  auto addCollections = [&](const char* key, AccessMode::Type t) {
    size_t numCollections = 0;
    builder.add(key, VPackValue(VPackValueType::Array));
    state.allCollections([&](TransactionCollection& col) {
      if (col.accessType() != t) {
        return true;
      }
      if (!state.isCoordinator()) {
        if (col.collection()->followers()->contains(server)) {
          builder.add(VPackValue(col.collectionName()));
          numCollections++;
        }
        return true; // continue
      }
      
      // coordinator starts transaction on shard leaders
#ifdef USE_ENTERPRISE
      if (col.collection()->isSmart() && col.collection()->type() == TRI_COL_TYPE_EDGE) {
        auto names = col.collection()->realNames();
        auto* ci = ClusterInfo::instance();
        for (std::string const& name : names) {
          auto cc = ci->getCollectionNT(state.vocbase().name(), name);
          if (!cc) {
            continue;
          }
          auto shards = ci->getShardList(std::to_string(cc->id()));
          for (ShardID const& shard : *shards) {
            auto sss = ci->getResponsibleServer(shard);
            if (server == sss->at(0)) {
              builder.add(VPackValue(shard));
              numCollections++;
            }
          }
        }
        return true; // continue
      }
#endif
      std::shared_ptr<ShardMap> shardIds = col.collection()->shardIds();
      for (auto const& pair : *shardIds) {
        TRI_ASSERT(!pair.second.empty());
        // only add shard where server is leader
        if (!pair.second.empty() && pair.second[0] == server) {
          builder.add(VPackValue(pair.first));
          numCollections++;
        }
      }
      return true;
    });
    builder.close();
    if (numCollections == 0) {
      builder.removeLast(); // no need to keep empty vals
    }
  };
  addCollections("read", AccessMode::Type::READ);
  addCollections("write", AccessMode::Type::WRITE);
  addCollections("exclusive", AccessMode::Type::EXCLUSIVE);

  builder.close();  // </collections>
  builder.close();  // </openObject>
}

/// @brief lazy begin a transaction on subordinate servers
ClusterCommRequest beginTransactionRequest(transaction::Methods const* trx,
                                           TransactionState& state, ServerID const& server) {
  TRI_voc_tid_t tid = state.id() + 1;
  TRI_ASSERT(!transaction::isLegacyTransactionId(tid));

  VPackBuilder builder;
  buildTransactionBody(state, server, builder);

  const std::string url = "/_db/" + StringUtils::urlEncode(state.vocbase().name()) +
                          "/_api/transaction/begin";
  auto headers = std::make_unique<std::unordered_map<std::string, std::string>>();
  headers->emplace(StaticStrings::ContentTypeHeader, StaticStrings::MimeTypeJson);
  headers->emplace(StaticStrings::TransactionId, std::to_string(tid));
  auto body = std::make_shared<std::string>(builder.slice().toJson());
  return ClusterCommRequest("server:" + server, RequestType::POST, url, body,
                            std::move(headers));
}

/// check the transaction cluster response with desited TID and status
Result checkTransactionResult(TRI_voc_tid_t desiredTid,
                              transaction::Status desStatus,
                              ClusterCommResult const& result) {
  Result res;

  int commError = ::handleGeneralCommErrors(&result);
  if (commError != TRI_ERROR_NO_ERROR) {
    // oh-oh cluster is in a bad state
    return res.reset(commError);
  }
  TRI_ASSERT(result.status == CL_COMM_RECEIVED);

  VPackSlice answer = result.answer->payload();
  if ((result.answer_code == rest::ResponseCode::OK ||
       result.answer_code == rest::ResponseCode::CREATED) &&
      answer.isObject()) {
    VPackSlice idSlice = answer.get(std::vector<std::string>{"result", "id"});
    VPackSlice statusSlice =
        answer.get(std::vector<std::string>{"result", "status"});

    if (!idSlice.isString() || !statusSlice.isString()) {
      return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                          "transaction has wrong format");
    }
    TRI_voc_tid_t tid = StringUtils::uint64(idSlice.copyString());
    VPackValueLength len = 0;
    const char* str = statusSlice.getStringUnchecked(len);
    if (tid == desiredTid && transaction::statusFromString(str, len) == desStatus) {
      return res;  // success
    }
  } else if (answer.isObject()) {
    std::string msg = std::string("error while ");
    if (desStatus == transaction::Status::RUNNING) {
      msg.append("beginning transaction");
    } else if (desStatus == transaction::Status::COMMITTED) {
      msg.append("committing transaction");
    } else if (desStatus == transaction::Status::ABORTED) {
      msg.append("aborting transaction");
    }                      
    return res.reset(VelocyPackHelper::readNumericValue(answer, StaticStrings::ErrorNum,
                                                        TRI_ERROR_TRANSACTION_INTERNAL),
                        VelocyPackHelper::getStringValue(answer, StaticStrings::ErrorMessage,
                                                         msg));
  }
  LOG_TOPIC("fb343", DEBUG, Logger::TRANSACTIONS)
      << " failed to begin transaction on " << result.endpoint;

  return res.reset(TRI_ERROR_TRANSACTION_INTERNAL);  // unspecified error
}

Result commitAbortTransaction(transaction::Methods& trx, transaction::Status status) {
  Result res;
  arangodb::TransactionState& state = *trx.state();
  TRI_ASSERT(state.isRunning());

  if (state.knownServers().empty()) {
    return res;
  }
  
  // only commit managed transactions, and AQL leader transactions (on DBServers)
  if (!ClusterTrxMethods::isElCheapo(state) ||
      (state.isCoordinator() && state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL))) {
    return res;
  }
  TRI_ASSERT(!state.isDBServer() || !transaction::isFollowerTransactionId(state.id()));

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return TRI_ERROR_SHUTTING_DOWN;
  }
  TRI_voc_tid_t tidPlus = state.id() + 1;
  // std::vector<ServerID> DBservers = ci->getCurrentDBServers();
  const std::string path = "/_db/" + StringUtils::urlEncode(state.vocbase().name()) +
                           "/_api/transaction/" + std::to_string(tidPlus);

  RequestType rtype;
  if (status == transaction::Status::COMMITTED) {
    rtype = RequestType::PUT;
  } else if (status == transaction::Status::ABORTED) {
    rtype = RequestType::DELETE_REQ;
  } else {
    TRI_ASSERT(false);
  }
  
  std::shared_ptr<std::string> body;
  std::vector<ClusterCommRequest> requests;
  for (std::string const& server : state.knownServers()) {
    LOG_TOPIC("d8457", DEBUG, Logger::TRANSACTIONS) << "Leader "
    << transaction::statusString(status) << " on " << server;
    requests.emplace_back("server:" + server, rtype, path, body);
  }
  
  // perform a synchronous commit on all leader servers
  cc->performRequests(requests, ::CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ false, /*retryOnBackUnvlbl*/ true);

  if (state.isCoordinator()) {
    TRI_ASSERT(transaction::isCoordinatorTransactionId(state.id()));

    for (size_t i = 0; i < requests.size(); ++i) {
      Result res = ::checkTransactionResult(tidPlus, status, requests[i].result);
      if (res.fail()) {
        return res;
      }
    }

    return res;
  } else {
    TRI_ASSERT(state.isDBServer());
    TRI_ASSERT(transaction::isLeaderTransactionId(state.id()));

    // Drop all followers that were not successful:
    for (size_t i = 0; i < requests.size(); ++i) {
      Result res = ::checkTransactionResult(tidPlus, status, requests[i].result);
      if (res.fail()) {  // remove follower from all collections
        ServerID const& follower = requests[i].result.serverID;
        state.allCollections([&](TransactionCollection& tc) {
          auto cc = tc.collection();
          if (cc) {
            if (cc->followers()->remove(follower)) {
              // TODO: what happens if a server is re-added during a transaction ?
              LOG_TOPIC("709c9", WARN, Logger::REPLICATION)
              << "synchronous replication: dropping follower " << follower
              << " for shard " << tc.collectionName();
            } else {
              LOG_TOPIC("4971f", ERR, Logger::REPLICATION)
              << "synchronous replication: could not drop follower "
              << follower << " for shard " << tc.collectionName();
              res.reset(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
              return false;  // cancel transaction
            }
          }
          return true;
        });
      }
    }
    return res;  // succeed even if some followers did not commit
  }
}
}  // namespace

namespace arangodb {

/// @brief begin a transaction on all leaders
arangodb::Result ClusterTrxMethods::beginTransactionOnLeaders(TransactionState& state,
                                                              std::vector<ServerID> const& leaders) {
  TRI_ASSERT(state.isCoordinator());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
  Result res;
  if (leaders.empty()) {
    return res;
  }

  std::vector<ClusterCommRequest> requests;
  for (ServerID const& leader : leaders) {
    if (state.knowsServer(leader)) {
      continue;  // already send a begin transaction there
    }
    state.addKnownServer(leader);
    requests.emplace_back(::beginTransactionRequest(nullptr, state, leader));
  }

  if (requests.empty()) {
    return res;
  }

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {
    // nullptr happens only during controlled shutdown
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // Perform the requests
  cc->performRequests(requests, ::CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                      /*retryOnCollNotFound*/ false, /*retryOnBackUnvlbl*/ true);

  TRI_voc_tid_t tid = state.id() + 1;
  for (size_t i = 0; i < requests.size(); ++i) {
    res = ::checkTransactionResult(tid, transaction::Status::RUNNING, requests[i].result);
    if (res.fail()) {  // remove follower from all collections
      return res;
    }
  }
  return res;  // all good
}

/// @brief begin a transaction on all followers
Result ClusterTrxMethods::beginTransactionOnFollowers(transaction::Methods& trx,
                                                      arangodb::FollowerInfo& info,
                                                      std::vector<ServerID> const& followers) {
  Result res;
  TransactionState& state = *trx.state();
  TRI_ASSERT(state.isDBServer());
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
  TRI_ASSERT(transaction::isLeaderTransactionId(state.id()));

  // prepare the requests:
  std::vector<ClusterCommRequest> requests;
  for (ServerID const& follower : followers) {
    if (state.knowsServer(follower)) {
      continue;  // already send a begin transaction there
    }
    state.addKnownServer(follower);
    requests.emplace_back(::beginTransactionRequest(&trx, state, follower));
  }

  if (requests.empty()) {
    return res;
  }

  auto cc = ClusterComm::instance();
  if (cc == nullptr) {  // nullptr happens only during controlled shutdown
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // Perform the requests
  size_t nrGood = cc->performRequests(requests, ::CL_DEFAULT_TIMEOUT, Logger::COMMUNICATION,
                                      /*retryOnCollNotFound*/ false);
  TRI_voc_tid_t tid = state.id(); // desired tid
  
  if (nrGood < followers.size()) {
    // Otherwise we drop all followers that were not successful:
    for (size_t i = 0; i < followers.size(); ++i) {
      Result r =
          ::checkTransactionResult(tid, transaction::Status::RUNNING, requests[i].result);
      if (r.fail()) {
        LOG_TOPIC("217e3", INFO, Logger::REPLICATION)
                  << "dropping follower because it did not start trx "
                  << state.id() << ", error: '" << r.errorMessage() << "'";
        if (info.remove(followers[i])) {
          // TODO: what happens if a server is re-added during a transaction ?
          LOG_TOPIC("c70a6", WARN, Logger::REPLICATION)
              << "synchronous replication: dropping follower " << followers[i];
        } else {
          LOG_TOPIC("fe8e1", ERR, Logger::REPLICATION)
              << "synchronous replication: could not drop follower " << followers[i];
          res.reset(TRI_ERROR_CLUSTER_COULD_NOT_DROP_FOLLOWER);
        }
      }
    }
  }
  // FIXME dropping followers is not asynchronous
  return res;
}

/// @brief commit a transaction on a subordinate
Result ClusterTrxMethods::commitTransaction(transaction::Methods& trx) {
  return commitAbortTransaction(trx, transaction::Status::COMMITTED);
}

/// @brief commit a transaction on a subordinate
Result ClusterTrxMethods::abortTransaction(transaction::Methods& trx) {
  return commitAbortTransaction(trx, transaction::Status::ABORTED);
}

/// @brief set the transaction ID header
void ClusterTrxMethods::addTransactionHeader(transaction::Methods const& trx,
                                             ServerID const& server,
                                             std::unordered_map<std::string, std::string>& headers) {
  TransactionState& state = *trx.state();
  TRI_ASSERT(state.isRunningInCluster());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;  // no need
  }
  TRI_voc_tid_t tidPlus = state.id() + 1;
  TRI_ASSERT(!transaction::isLegacyTransactionId(tidPlus));
  TRI_ASSERT(!state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));

  const bool addBegin = !state.knowsServer(server);
  if (addBegin) {
    if (state.isCoordinator() &&
        state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
      return;  // do not add header to servers without a snippet
    }
    TRI_ASSERT(state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED) ||
               transaction::isLeaderTransactionId(state.id()));
    transaction::BuilderLeaser builder(trx.transactionContextPtr());
    ::buildTransactionBody(state, server, *builder.get());
    headers.emplace(StaticStrings::TransactionBody, builder->toJson());
    headers.emplace(arangodb::StaticStrings::TransactionId,
                    std::to_string(tidPlus).append(" begin"));
    state.addKnownServer(server);  // remember server
  } else {
    headers.emplace(arangodb::StaticStrings::TransactionId, std::to_string(tidPlus));
  }
}

/// @brief add transaction ID header for setting up AQL snippets
void ClusterTrxMethods::addAQLTransactionHeader(transaction::Methods const& trx,
                                                ServerID const& server,
                                                std::unordered_map<std::string, std::string>& headers) {
  TransactionState& state = *trx.state();
  TRI_ASSERT(state.isCoordinator());
  if (!ClusterTrxMethods::isElCheapo(trx)) {
    return;
  }

  std::string value = std::to_string(state.id() + 1);
  const bool addBegin = !state.knowsServer(server);
  if (addBegin) {
    if (state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL)) {
      value.append(" aql"); // This is a single AQL query
    } else if (state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
      transaction::BuilderLeaser builder(trx.transactionContextPtr());
      ::buildTransactionBody(state, server, *builder.get());
      headers.emplace(StaticStrings::TransactionBody, builder->toJson());
      value.append(" begin"); // part of a managed transaction
    } else {
      TRI_ASSERT(false);
    }
    state.addKnownServer(server);  // remember server
  }
  headers.emplace(arangodb::StaticStrings::TransactionId, std::move(value));
}

bool ClusterTrxMethods::isElCheapo(transaction::Methods const& trx) {
  return ClusterTrxMethods::isElCheapo(*trx.state());
}

bool ClusterTrxMethods::isElCheapo(TransactionState const& state) {
  return !transaction::isLegacyTransactionId(state.id()) &&
         (state.hasHint(transaction::Hints::Hint::GLOBAL_MANAGED) ||
          state.hasHint(transaction::Hints::Hint::FROM_TOPLEVEL_AQL));
}

}  // namespace arangodb
