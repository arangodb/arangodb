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
#include "Cluster/ClusterMethods.h"
#include "Cluster/ClusterTrxMethods.h"
#include "ClusterEngine/ClusterEngine.h"
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
    : TransactionState(vocbase, tid, options) {}

/// @brief start a transaction
Result ClusterTransactionState::beginTransaction(transaction::Hints hints) {
  LOG_TRX("03dec", TRACE, this, nestingLevel())
      << "beginning " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(!hasHint(transaction::Hints::Hint::NO_USAGE_LOCK) ||
             !AccessMode::isWriteOrExclusive(_type));

  if (nestingLevel() == 0) {
    // set hints
    _hints = hints;
  }
  
  auto cleanup = [&] {
    if (nestingLevel() == 0) {
      updateStatus(transaction::Status::ABORTED);
    }
    // free what we have got so far
    unuseCollections(nestingLevel());
  };

  Result res = useCollections(nestingLevel());
  if (res.fail()) { // something is wrong
    cleanup();
    return res;
  }
  
  // all valid
  if (nestingLevel() == 0) {
    updateStatus(transaction::Status::RUNNING);
    
    transaction::ManagerFeature::manager()->registerTransaction(id(), nullptr);
    setRegistered();
    
    ClusterEngine* ce = static_cast<ClusterEngine*>(EngineSelectorFeature::ENGINE);
    if (ce->isMMFiles() && hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
      TRI_ASSERT(isCoordinator());
      
      std::vector<std::string> leaders;
      allCollections([&leaders](TransactionCollection& c) {
        auto shardIds = c.collection()->shardIds();
        for (auto const& pair : *shardIds) {
          std::vector<arangodb::ShardID> const& servers = pair.second;
          if (!servers.empty()) {
            leaders.push_back(servers[0]);
          }
        }
        return true; // continue
      });
      
      res = ClusterTrxMethods::beginTransactionOnLeaders(*this, leaders);
      if (res.fail()) { // something is wrong
        cleanup();
      }
    }
    
  } else {
    TRI_ASSERT(_status == transaction::Status::RUNNING);
  }

  return res;
}

/// @brief commit a transaction
Result ClusterTransactionState::commitTransaction(transaction::Methods* activeTrx) {
  LOG_TRX("927c0", TRACE, this, nestingLevel())
      << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }

  arangodb::Result res;
  if (nestingLevel() == 0) {
    updateStatus(transaction::Status::COMMITTED);
  }

  unuseCollections(nestingLevel());
  return res;
}

/// @brief abort and rollback a transaction
Result ClusterTransactionState::abortTransaction(transaction::Methods* activeTrx) {
  LOG_TRX("fc653", TRACE, this, nestingLevel()) << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  Result res;
  if (nestingLevel() == 0) {
    updateStatus(transaction::Status::ABORTED);
  }

  unuseCollections(nestingLevel());
  return res;
}
