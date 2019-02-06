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

#include "TransactionManager.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"

using namespace arangodb;

bool TransactionManager::isChildTransactionId(TRI_voc_tid_t tid) {
  return isLeaderTransactionId(tid) || isFollowerTransactionId(tid);
}

bool TransactionManager::isCoordinatorTransactionId(TRI_voc_tid_t tid) {
  return (tid % 4) == 0;
}

bool TransactionManager::isFollowerTransactionId(TRI_voc_tid_t tid) {
  return (tid % 4) == 2;
}

bool TransactionManager::isLeaderTransactionId(TRI_voc_tid_t tid) {
  return (tid % 4) == 1;
}

bool TransactionManager::isLegacyTransactionId(TRI_voc_tid_t tid) {
  return (tid % 4) == 3;
}

// register a list of failed transactions
void TransactionManager::registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  for (auto const& it : failedTransactions) {
    size_t bucket = getBucket(it);

    WRITE_LOCKER(locker, _transactions[bucket]._lock);

    _transactions[bucket]._failedTransactions.emplace(it);
  }
}

// unregister a list of failed transactions
void TransactionManager::unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    WRITE_LOCKER(locker, _transactions[bucket]._lock);

    std::for_each(failedTransactions.begin(), failedTransactions.end(), [&](TRI_voc_tid_t id) {
      _transactions[bucket]._failedTransactions.erase(id);
    });
  }
}

void TransactionManager::registerTransaction(TransactionState& state,
                                             std::unique_ptr<TransactionData> data) {
  TRI_ASSERT(data != nullptr);
  _nrRunning.fetch_add(1, std::memory_order_relaxed);

  bool isManaged = state.hasHint(transaction::Hints::Hint::MANAGED);
  TRI_ASSERT(!isManaged || !state.hasHint(transaction::Hints::Hint::SINGLE_OPERATION));
  if (!isManaged && !keepTransactionData(state)) {
    return;
  }

  // LOG_DEVEL << "Registering transaction " << state.id();
  size_t bucket = getBucket(state.id());
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  if (isManaged) {
    data->_state = &state;
    data->_expires = TRI_microtime() + defaultTTL;
    if (_transactions[bucket]._activeTransactions.find(state.id()) !=
        _transactions[bucket]._activeTransactions.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                     "Duplicate transaction ID");
    }
  }

  // insert into currently running list of transactions
  _transactions[bucket]._activeTransactions.emplace(state.id(), std::move(data));
}

// unregisters a transaction
void TransactionManager::unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed) {
  _nrRunning.fetch_sub(1, std::memory_order_relaxed);

  size_t bucket = getBucket(transactionId);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  _transactions[bucket]._activeTransactions.erase(transactionId);
  if (markAsFailed) {
    _transactions[bucket]._failedTransactions.emplace(transactionId);
  }
  /*size_t numRemoved =
   if (numRemoved > 0) {
    LOG_DEVEL << "Unregistering transaction " << transactionId
              << (markAsFailed ? ": abort" : " commit");
  }*/
}

// return the set of failed transactions
std::unordered_set<TRI_voc_tid_t> TransactionManager::getFailedTransactions() const {
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

void TransactionManager::iterateActiveTransactions(
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

uint64_t TransactionManager::getActiveTransactionCount() {
  return _nrRunning.load(std::memory_order_relaxed);
  /*WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

  uint64_t count = 0;
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);
    count += _transactions[bucket]._activeTransactions.size();
  }
  return count;*/
}

/// @brief lease the transaction, increases nesting
TransactionState* TransactionManager::lookup(TRI_voc_tid_t transactionId,
                                             TransactionManager::Ownership action) const {
  size_t bucket = getBucket(transactionId);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  READ_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto const& buck = _transactions[bucket];
  auto const& it = buck._activeTransactions.find(transactionId);
  if (it != buck._activeTransactions.end()) {
    if (it->second->_state != nullptr) {  // nullptr means unmanaged
      TransactionState* const state = it->second->_state;
      TRI_ASSERT(state->hasHint(transaction::Hints::Hint::MANAGED));
      if (state->isEmbeddedTransaction()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                       "Concurrent use of transaction");
      }
      if (action == Ownership::Lease) {
        it->second->_expires = defaultTTL + TRI_microtime();
        state->increaseNesting();
      } else /*if (action == Ownership::Move)*/ {
        it->second->_state = nullptr;  // no longer manage this
      }
      return state;
    }
  }

  // as recourse check if this is a known failed transaction
  auto const& it2 = buck._failedTransactions.find(transactionId);
  if (it2 != buck._failedTransactions.end()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_ABORTED);
  }

  return nullptr;
}

struct GCTransaction final : transaction::Methods {
  GCTransaction(std::shared_ptr<transaction::SmartContext> const& ctx,
                transaction::Options const& opts)
      : Methods(ctx, opts) {
    TRI_ASSERT(_state->isEmbeddedTransaction());
  }
};

/// @brief collect forgotten transactions
void TransactionManager::garbageCollect() {
  std::vector<TRI_voc_tid_t> gcBuffer;

  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    WRITE_LOCKER(locker, _transactions[bucket]._lock);

    double now = TRI_microtime();
    auto it = _transactions[bucket]._activeTransactions.begin();
    while (it != _transactions[bucket]._activeTransactions.end()) {
      // we only concern ourselves with global transactions
      if (it->second->_state != nullptr) {
        TransactionState* state = it->second->_state;
        TRI_ASSERT(state->hasHint(transaction::Hints::Hint::MANAGED));
        if (state->isTopLevelTransaction()) {  // embedded means leased out
          // aborted transactions must be cleanead up by the aborting thread
          if (state->isRunning() && it->second->_expires > now) {
            gcBuffer.emplace_back(state->id());
          }
        } else {
          // auto extend lifetime of leased transactions
          it->second->_expires = defaultTTL + TRI_microtime();
        }
      }
      ++it;  // next
    }
  }

  /*const TRI_voc_tid_t tid = state->id();
   auto ctx =
   std::make_shared<transaction::SmartContext>(state->vocbase(),
   state);
   transaction::Options trxOpts;
   GCTransaction trx(ctx, trxOpts);
   TRI_ASSERT(trx.state()->status() == transaction::Status::RUNNING);
   Result res = trx.abort(); // should delete state
   if (res.ok()) {
   LOG_TOPIC(WARN, Logger::TRANSACTIONS) << "failed to GC
   transaction: " << res.errorMessage();
   }
   it = _transactions[bucket]._activeTransactions.erase(it);
   _transactions[bucket]._failedTransactions.emplace(tid);*/
}
