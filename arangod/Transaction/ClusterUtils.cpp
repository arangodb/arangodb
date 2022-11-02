////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "ClusterUtils.h"
#include "Cluster/ClusterHelpers.h"
#include "Cluster/ClusterInfo.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
namespace transaction {
namespace cluster {

void abortTransactions(LogicalCollection& coll) {
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  if (!mgr) {  // might be called during shutdown
    return;
  }

  bool didWork =
      mgr->abortManagedTrx([&coll](TransactionState const& state,
                                   std::string const& /*user*/) -> bool {
        TransactionCollection* tcoll =
            state.collection(coll.id(), AccessMode::Type::NONE);
        return tcoll != nullptr;
      });

  LOG_TOPIC_IF("7eda2", INFO, Logger::TRANSACTIONS, didWork)
      << "aborted leader transactions on shard " << coll.id() << "'";
}

void abortLeaderTransactionsOnShard(DataSourceId cid) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool didWork =
      mgr->abortManagedTrx([cid](TransactionState const& state,
                                 std::string const& /*user*/) -> bool {
        if (state.id().isLeaderTransactionId()) {
          TransactionCollection* tcoll =
              state.collection(cid, AccessMode::Type::NONE);
          return tcoll != nullptr;
        }
        return false;
      });

  LOG_TOPIC_IF("7edb3", INFO, Logger::TRANSACTIONS, didWork)
      << "aborted leader transactions on shard '" << cid << "'";
}

void abortFollowerTransactionsOnShard(DataSourceId cid) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool didWork =
      mgr->abortManagedTrx([cid](TransactionState const& state,
                                 std::string const& /*user*/) -> bool {
        if (state.id().isFollowerTransactionId()) {
          TransactionCollection* tcoll =
              state.collection(cid, AccessMode::Type::NONE);
          return tcoll != nullptr;
        }
        return false;
      });

  LOG_TOPIC_IF("7dcff", INFO, Logger::TRANSACTIONS, didWork)
      << "aborted follower transactions on shard '" << cid << "'";
}

void abortTransactionsWithFailedServers(ClusterInfo& ci) {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());

  auto failedServers = ci.getFailedServers();
  transaction::Manager* mgr = transaction::ManagerFeature::manager();
  TRI_ASSERT(mgr != nullptr);

  bool didWork = false;
  if (ServerState::instance()->isCoordinator()) {
    // abort all transactions using a lead server
    didWork = mgr->abortManagedTrx([&](TransactionState const& state,
                                       std::string const& /*user*/) -> bool {
      for (auto const& sid : failedServers) {
        if (state.knowsServer(sid)) {
          return true;
        }
      }
      return false;
    });

  } else if (ServerState::instance()->isDBServer()) {
    // only care about failed coordinators
    size_t coordinatorsCount = 0;
    for (auto const& server : failedServers) {
      if (ClusterHelpers::isCoordinatorName(server)) {
        ++coordinatorsCount;
      }
    }
    if (coordinatorsCount == 0) {
      return;
    }

    // abort all transaction started by a certain coordinator
    didWork = mgr->abortManagedTrx([&](TransactionState const& state,
                                       std::string const& /*user*/) -> bool {
      uint32_t serverId = state.id().serverId();
      if (serverId != 0) {
        auto coordId = ci.getCoordinatorByShortID(serverId);
        return failedServers.contains(coordId);
      }
      return false;
    });
  }

  LOG_TOPIC_IF("b59e3", INFO, Logger::TRANSACTIONS, didWork)
      << "aborting transactions for servers '" << failedServers << "'";
}

}  // namespace cluster
}  // namespace transaction
}  // namespace arangodb
