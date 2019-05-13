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
#include "Basics/Exceptions.h"
#include "Logger/Logger.h"
#include "RestServer/TransactionManagerFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionCollection.h"
#include "StorageEngine/TransactionManager.h"
#include "Transaction/Methods.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;

// for the Cluster engine we do not need any additional data
struct ClusterTransactionData final : public TransactionData {};

/// @brief transaction type
ClusterTransactionState::ClusterTransactionState(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                                                 transaction::Options const& options)
    : TransactionState(vocbase, tid, options) {}

/// @brief free a transaction container
ClusterTransactionState::~ClusterTransactionState() {}

/// @brief start a transaction
Result ClusterTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX(this, _nestingLevel)
      << "beginning " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(!hasHint(transaction::Hints::Hint::NO_USAGE_LOCK) ||
             !AccessMode::isWriteOrExclusive(_type));

  if (_nestingLevel == 0) {
    // set hints
    _hints = hints;
  }

  Result result = useCollections(_nestingLevel);
  if (result.ok()) {
    // all valid
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::RUNNING);
    }
  } else {
    // something is wrong
    if (_nestingLevel == 0) {
      updateStatus(transaction::Status::ABORTED);
    }

    // free what we have got so far
    unuseCollections(_nestingLevel);

    return result;
  }

  if (_nestingLevel == 0) {
    // register a protector (intentionally empty)
    auto data = std::make_unique<ClusterTransactionData>();
    TransactionManagerFeature::manager()->registerTransaction(_id, std::move(data), isReadOnlyTransaction());
    setRegistered();

  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }

  return result;
}

/// @brief commit a transaction
Result ClusterTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel)
      << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }

  arangodb::Result res;
  if (_nestingLevel == 0) {
    if (true) {
      updateStatus(transaction::Status::COMMITTED);
      // cleanupTransaction();  // deletes trx
    } else {
      abortTransaction(activeTrx);  // deletes trx
    }
  }

  unuseCollections(_nestingLevel);
  return res;
}

/// @brief abort and rollback a transaction
Result ClusterTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX(this, _nestingLevel) << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  Result result;
  if (_nestingLevel == 0) {
    updateStatus(transaction::Status::ABORTED);
  }

  unuseCollections(_nestingLevel);
  return result;
}
