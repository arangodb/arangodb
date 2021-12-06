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

#include "ClusterTransactionState.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterTrxMethods.h"
#include "ClusterEngine/ClusterEngine.h"
#include "ClusterEngine/ClusterTransactionCollection.h"
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Counter.h"
#include "Metrics/MetricsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief transaction type
ClusterTransactionState::ClusterTransactionState(TRI_vocbase_t& vocbase, TransactionId tid,
                                                 transaction::Options const& options)
    : TransactionState(vocbase, tid, options) {
  TRI_ASSERT(isCoordinator());
  // we have to read revisions here as validateAndOptimize is executed before
  // transaction is started and during validateAndOptimize some simple
  // function calls could be executed and calls requires valid analyzers revisions.
  acceptAnalyzersRevision(
      _vocbase.server().getFeature<arangodb::ClusterFeature>()
        .clusterInfo().getQueryAnalyzersRevision(vocbase.name()));
}

/// @brief start a transaction
Result ClusterTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX("03dec", TRACE, this)
      << "beginning " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(!hasHint(transaction::Hints::Hint::NO_USAGE_LOCK) ||
             !AccessMode::isWriteOrExclusive(_type));
  TRI_ASSERT(_status == transaction::Status::CREATED);

  // set hints
  _hints = hints;
  auto& stats = _vocbase.server().getFeature<metrics::MetricsFeature>().serverStatistics()._transactionsStatistics;

  auto cleanup = scopeGuard([&]() noexcept {
    updateStatus(transaction::Status::ABORTED);
    ++stats._transactionsAborted;
  });

  Result res = useCollections();
  if (res.fail()) { // something is wrong
    return res;
  }

  // all valid
  updateStatus(transaction::Status::RUNNING);
  if (isReadOnlyTransaction()) {
    ++stats._readTransactions;
  } else {
    ++stats._transactionsStarted;
  }

  transaction::ManagerFeature::manager()->registerTransaction(id(), isReadOnlyTransaction(), false /* isFollowerTransaction */);
  setRegistered();
  if (AccessMode::isWriteOrExclusive(this->_type) &&
      hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    TRI_ASSERT(isCoordinator());

    ClusterTrxMethods::SortedServersSet leaders{};
    allCollections([&](TransactionCollection& c) {
      auto shardIds = c.collection()->shardIds();
      for (auto const& pair : *shardIds) {
        std::vector<arangodb::ShardID> const& servers = pair.second;
        if (!servers.empty()) {
          leaders.emplace(servers[0]);
        }
      }
      return true;  // continue
    });

    // if there is only one server we may defer the lazy locking
    // until the first actual operation (should save one request)
    if (leaders.size() > 1) {
      res = ClusterTrxMethods::beginTransactionOnLeaders(*this, leaders,
                                                         transaction::MethodsApi::Synchronous)
                .get();
      if (res.fail()) {  // something is wrong
        return res;
      }
    }
  }

  cleanup.cancel();
  return res;
}

/// @brief commit a transaction
Result ClusterTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  LOG_TRX("927c0", TRACE, this)
      << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }

  updateStatus(transaction::Status::COMMITTED);
  ++_vocbase.server().getFeature<metrics::MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsCommitted;

  return {};
}

/// @brief abort and rollback a transaction
Result ClusterTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX("fc653", TRACE, this) << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);

  updateStatus(transaction::Status::ABORTED);
  ++_vocbase.server().getFeature<metrics::MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsAborted;
  
  return {};
}
  
Result ClusterTransactionState::performIntermediateCommitIfRequired(DataSourceId cid) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "unexpected intermediate commit");
}
  
/// @brief return number of commits
uint64_t ClusterTransactionState::numCommits() const {
  // there are no intermediate commits for a cluster transaction, so we can
  // return 1 for a committed transaction and 0 otherwise
  return _status == transaction::Status::COMMITTED ? 1 : 0;
}

TRI_voc_tick_t ClusterTransactionState::lastOperationTick() const noexcept {
  return 0;
}

std::unique_ptr<TransactionCollection> ClusterTransactionState::createTransactionCollection(
    DataSourceId cid, AccessMode::Type accessType) {
  return std::make_unique<ClusterTransactionCollection>(this, cid, accessType);
}
