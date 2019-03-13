////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Manager.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Utils/CollectionNameResolver.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {
namespace transaction {

// register a list of failed transactions
void Manager::registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  TRI_ASSERT(_keepTransactionData);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (auto const& it : failedTransactions) {
    const size_t bucket = getBucket(it);
    WRITE_LOCKER(locker, _transactions[bucket]._lock);
    _transactions[bucket]._failedTransactions.emplace(it);
  }
}

// unregister a list of failed transactions
void Manager::unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  TRI_ASSERT(_keepTransactionData);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    WRITE_LOCKER(locker, _transactions[bucket]._lock);
    std::for_each(failedTransactions.begin(), failedTransactions.end(), [&](TRI_voc_tid_t id) {
      _transactions[bucket]._failedTransactions.erase(id);
    });
  }
}

void Manager::registerTransaction(TRI_voc_tid_t transactionId,
                                  std::unique_ptr<TransactionData> data) {
  _nrRunning.fetch_add(1, std::memory_order_relaxed);

  if (_keepTransactionData) {
    TRI_ASSERT(data != nullptr);
    const size_t bucket = getBucket(transactionId);
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    
    try {
      // insert into currently running list of transactions
      _transactions[bucket]._activeTransactions.emplace(transactionId, std::move(data));
    } catch (...) {
      _nrRunning.fetch_sub(1, std::memory_order_relaxed);
      throw;
    }
  }
}

// unregisters a transaction
void Manager::unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed) {
  uint64_t r = _nrRunning.fetch_sub(1, std::memory_order_relaxed);
  TRI_ASSERT(r > 0);

  if (_keepTransactionData) {
    const size_t bucket = getBucket(transactionId);
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    _transactions[bucket]._activeTransactions.erase(transactionId);
    if (markAsFailed) {
      _transactions[bucket]._failedTransactions.emplace(transactionId);
    }
  }
}

// return the set of failed transactions
std::unordered_set<TRI_voc_tid_t> Manager::getFailedTransactions() const {
  std::unordered_set<TRI_voc_tid_t> failedTransactions;

  {
    WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      READ_LOCKER(locker, _transactions[bucket]._lock);

      for (auto const& it : _transactions[bucket]._failedTransactions) {
        failedTransactions.emplace(it);
      }
    }
  }

  return failedTransactions;
}

void Manager::iterateActiveTransactions(
    std::function<void(TRI_voc_tid_t, TransactionData const*)> const& callback) {
  WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    for (auto const& it : _transactions[bucket]._activeTransactions) {
      callback(it.first, it.second.get());
    }
  }
}

uint64_t Manager::getActiveTransactionCount() {
  return _nrRunning.load(std::memory_order_relaxed);
}

}  // namespace transaction
}  // namespace arangodb
