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

#include "ClusterUtils.h"

#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"

namespace arangodb {
namespace transaction {
namespace cluster {

void abortTransactions(LogicalCollection& coll) {
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  if (!mgr) { // might be called during shutdown
    return;
  }

  bool didWork = mgr->abortManagedTrx(
      [&coll](TransactionState const& state, std::string const & /*user*/) -> bool {
    TransactionCollection* tcoll = state.collection(coll.id(), AccessMode::Type::NONE);
        return tcoll != nullptr;
      });
  LOG_TOPIC_IF("7eda2", INFO, Logger::TRANSACTIONS, didWork) <<
  "aborted leader transactions on shard '" << coll.id() << "'";
}

void abortLeaderTransactionsOnShard(TRI_voc_cid_t cid) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool didWork = mgr->abortManagedTrx(
      [cid](TransactionState const& state, std::string const & /*user*/) -> bool {
        if (transaction::isLeaderTransactionId(state.id())) {
          TransactionCollection* tcoll = state.collection(cid, AccessMode::Type::NONE);
          return tcoll != nullptr;
        }
        return false;
      });
  LOG_TOPIC_IF("7edb3", INFO, Logger::TRANSACTIONS, didWork) <<
  "aborted leader transactions on shard '" << cid << "'";
}

void abortFollowerTransactionsOnShard(TRI_voc_cid_t cid) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool didWork = mgr->abortManagedTrx(
      [cid](TransactionState const& state, std::string const & /*user*/) -> bool {
        if (transaction::isFollowerTransactionId(state.id())) {
          TransactionCollection* tcoll = state.collection(cid, AccessMode::Type::NONE);
          return tcoll != nullptr;
        }
        return false;
      });
  LOG_TOPIC_IF("7dcff", INFO, Logger::TRANSACTIONS, didWork) <<
  "aborted follower transactions on shard '" << cid << "'";
}

void abortTransactionsWithFailedServers(ClusterInfo& ci) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());

  std::vector<ServerID> failed = ci.getFailedServers();
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);
  
  bool didWork = false;
  if (ServerState::instance()->isCoordinator()) {
    
    // abort all transactions using a lead server
    didWork = mgr->abortManagedTrx([&](TransactionState const& state, std::string const& /*user*/) -> bool {
      for (ServerID const& sid : failed) {
        if (state.knowsServer(sid)) {
          return true;
        }
      }
      return false;
    });
    
  } else if (ServerState::instance()->isDBServer()) {
    
    // only care about failed coordinators
    failed.erase(std::remove_if(failed.begin(), failed.end(), [](ServerID const& str) {
      return str.compare(0, 4, "CRDN") != 0;
    }), failed.end());
    if (failed.empty()) {
      return;
    }
    
    // abort all transaction started by a certain coordinator
    didWork = mgr->abortManagedTrx([&](TransactionState const& state, std::string const& /*user*/) -> bool {
      uint32_t serverId = TRI_ExtractServerIdFromTick(state.id());
      if (serverId != 0) {
        ServerID coordId = ci.getCoordinatorByShortID(serverId);
        return std::find(failed.begin(), failed.end(), coordId) != failed.end();
      }
      return false;
    });
  }
  
  LOG_TOPIC_IF("b59e3", INFO, Logger::TRANSACTIONS, didWork) <<
  "aborting transactions for servers '" << failed << "'";
}

}  // namespace cluster
}  // namespace transaction
}  // namespace arangodb
