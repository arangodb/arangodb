////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
#include "IResearch/IResearchAnalyzerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/MetricsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

/// @brief transaction type
ClusterTransactionState::ClusterTransactionState(TRI_vocbase_t& vocbase,
                                                 TRI_voc_tid_t tid,
                                                 transaction::Options const& options)
    : TransactionState(vocbase, tid, options) {
  TRI_ASSERT(isCoordinator());
  acceptAnalyzersRevision(_vocbase.server()
    .getFeature< arangodb::iresearch::IResearchAnalyzerFeature>()
    .getAnalyzersRevision(_vocbase, true)->getRevision());
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

  auto cleanup = scopeGuard([&] {
    updateStatus(transaction::Status::ABORTED);
    _vocbase.server().getFeature<MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsAborted++;
  });

  Result res = useCollections();
  if (res.fail()) { // something is wrong
    return res;
  }

  // all valid
  updateStatus(transaction::Status::RUNNING);
  _vocbase.server().getFeature<MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsStarted++;

  transaction::ManagerFeature::manager()->registerTransaction(id(), isReadOnlyTransaction());
  setRegistered();

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

  arangodb::Result res;
  
  updateStatus(transaction::Status::COMMITTED);
  _vocbase.server().getFeature<MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsCommitted++;

  return res;
}

/// @brief abort and rollback a transaction
Result ClusterTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX("fc653", TRACE, this) << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);

  updateStatus(transaction::Status::ABORTED);
  _vocbase.server().getFeature<MetricsFeature>().serverStatistics()._transactionsStatistics._transactionsAborted++;
  
  return Result();
}
