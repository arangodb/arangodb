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
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>


namespace arangodb {
namespace transaction {
  
namespace {
  struct MGMethods final : arangodb::transaction::Methods {
    MGMethods(std::shared_ptr<arangodb::transaction::Context> const& ctx,
              arangodb::transaction::Options const& opts)
    : Methods(ctx, opts) {
      TRI_ASSERT(_state->isEmbeddedTransaction());
    }
  };
}
  
// register a list of failed transactions
void Manager::registerFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  if (_keepTransactionData) {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    for (auto const& it : failedTransactions) {
      const size_t bucket = getBucket(it);
      WRITE_LOCKER(locker, _transactions[bucket]._lock);
      _transactions[bucket]._failedTransactions.emplace(it);
    }
  }
}

// unregister a list of failed transactions
void Manager::unregisterFailedTransactions(std::unordered_set<TRI_voc_tid_t> const& failedTransactions) {
  if (_keepTransactionData) {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
      WRITE_LOCKER(locker, _transactions[bucket]._lock);
      std::for_each(failedTransactions.begin(), failedTransactions.end(), [&](TRI_voc_tid_t id) {
        _transactions[bucket]._failedTransactions.erase(id);
      });
    }
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
    
    // insert into currently running list of transactions
    _transactions[bucket]._activeTransactions.emplace(transactionId, std::move(data));
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
  /*WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);
  uint64_t count = 0;
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);
    count += _transactions[bucket]._activeTransactions.size();
  }
  return count;*/
}

Manager::ManagedTrx::~ManagedTrx() {
  if (type == MetaType::StandaloneAQL || state->isEmbeddedTransaction()) {
    return;
  }
  transaction::Options opts;
  auto ctx = std::make_shared<transaction::ManagedContext>(2, state, AccessMode::Type::NONE);
  MGMethods trx(ctx, opts); // own state now
  trx.begin();
  TRI_ASSERT(state->isTopLevelTransaction());
  trx.abort(); //
}
  
using namespace arangodb;
  
/// @brief collect forgotten transactions
void Manager::garbageCollect() {
#warning TODO garbage collection
//  std::vector<TRI_voc_tid_t> gcBuffer;
//
//  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
//  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
//    WRITE_LOCKER(locker, _transactions[bucket]._lock);
//
//    double now = TRI_microtime();
//    auto it = _transactions[bucket]._activeTransactions.begin();
//    while (it != _transactions[bucket]._activeTransactions.end()) {
//      // we only concern ourselves with global transactions
//      if (it->second->_state != nullptr) {
//        TransactionState* state = it->second->_state;
//        TRI_ASSERT(!Manager::isManagedTransactionId(state->id()));
//        if (state->isTopLevelTransaction()) {  // embedded means leased out
//          // aborted transactions must be cleanead up by the aborting thread
//          if (state->isRunning() && it->second->_expires > now) {
//            gcBuffer.emplace_back(state->id());
//          }
//        } else {
//          // auto extend lifetime of leased transactions
//          it->second->_expires = defaultTTL + TRI_microtime();
//        }
//      }
//      ++it;  // next
//    }
//  }

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

/// @brief register a transaction shard
/// @brief tid global transaction shard
/// @param cid the optional transaction ID (use 0 for a single shard trx)
void Manager::registerAQLTrx(TransactionState* state) {
  TRI_ASSERT(state != nullptr);
  const size_t bucket = getBucket(state->id());
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
  
  auto& buck = _transactions[bucket];
  auto it = buck._managed.find(state->id());
  if (it != buck._managed.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                   "transaction ID already used");
  }
  
  buck._managed.emplace(std::piecewise_construct,
                        std::forward_as_tuple(state->id()),
                        std::forward_as_tuple(MetaType::StandaloneAQL, state,
                                              (defaultTTL + TRI_microtime())));
}
  
void Manager::unregisterAQLTrx(TRI_voc_tid_t tid) noexcept {
  const size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
  
  auto& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  if (it == buck._managed.end()) {
    LOG_TOPIC(ERR, Logger::TRANSACTIONS) << "a registered transaction was not found";
    TRI_ASSERT(false);
    return;
  }
  TRI_ASSERT(it->second.type == MetaType::StandaloneAQL);
  
  if (!it->second.rwlock.writeLock(/*maxAttempts*/256)) {
    LOG_TOPIC(ERR, Logger::TRANSACTIONS) << "a transaction is still in use";
    TRI_ASSERT(false);
    return;
  }
  buck._managed.erase(it);
}
  
Result Manager::createManagedTrx(TRI_vocbase_t& vocbase,
                                 TRI_voc_tid_t tid, VPackSlice const trxOpts) {
  const size_t bucket = getBucket(tid);
  
  { // quick check whether ID exists
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                     "transaction ID already used");
    }
  }
  
  Result res;
  // extract the properties from the object
  transaction::Options options;
  options.fromVelocyPack(trxOpts);
  if (options.lockTimeout < 0.0) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "<lockTiemout> needs to be positive");
  }
  
  // parse the collections to register
  if (!trxOpts.isObject() || !trxOpts.get("collections").isObject()) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "missing 'collections'");
  }
  auto fillColls = [](VPackSlice const& slice, std::vector<std::string>& cols) {
    if (slice.isNone()) {  // ignore nonexistant keys
      return true;
    } else if (!slice.isArray()) {
      return false;
    }
    for (VPackSlice val : VPackArrayIterator(slice)) {
      if (!val.isString() || val.getStringLength() == 0) {
        return false;
      }
      cols.emplace_back(val.copyString());
    }
    return true;
  };
  std::vector<std::string> reads, writes, exlusives;
  VPackSlice collections = trxOpts.get("collections");
  bool isValid = fillColls(collections.get("read"), reads) &&
  fillColls(collections.get("write"), writes) &&
  fillColls(collections.get("exclusive"), exlusives);
  if (!isValid) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "invalid 'collections'");
  }
  
  // now start our own transaction
  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  auto state = engine->createTransactionState(vocbase, tid, options);
  TRI_ASSERT(state != nullptr);
  
  // lock collections
  CollectionNameResolver resolver(vocbase);
  auto lockCols = [&](std::vector<std::string> cols, AccessMode::Type mode) {
    for (auto const& cname : cols) {
      TRI_voc_cid_t cid = 0;
      if (state->isCoordinator()) {
        cid = resolver.getCollectionIdCluster(cname);
      } else {  // only support local collections / shards
        cid = resolver.getCollectionIdLocal(cname);
      }
      if (cid == 0) {
        LOG_DEVEL << "collection " << cname << " not found";
        return false;
      }
      state->addCollection(cid, cname, mode, /*nestingLevel*/0, false);
    }
    return true;
  };
  if (!lockCols(exlusives, AccessMode::Type::EXCLUSIVE) ||
      !lockCols(writes, AccessMode::Type::WRITE) ||
      !lockCols(reads, AccessMode::Type::READ)) {
    return res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  
  // start the transaction
  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::LOCK_ENTIRELY);
  hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
  res = state->beginTransaction(hints);  // registers with transaction manager
  if (res.fail()) {
    return res;
  }
  
  { // add transaction to
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      return res.reset(TRI_ERROR_TRANSACTION_INTERNAL, "transaction ID already used");
    }
    buck._managed.emplace(std::piecewise_construct,
                          std::forward_as_tuple(state->id()),
                          std::forward_as_tuple(MetaType::Managed, state.release(),
                                                (defaultTTL + TRI_microtime())));
  }
  return res;
}

/// @brief lease the transaction, increases nesting
std::shared_ptr<transaction::Context> Manager::leaseTrx(TRI_voc_tid_t tid, AccessMode::Type mode,
                                                        Manager::Ownership action) {
  const size_t bucket = getBucket(tid);
  
  TransactionState* state = nullptr;
  do {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end()) {
      // as last recourse check if this is a known failed transaction
      auto const& it2 = buck._failedTransactions.find(tid);
      if (it2 != buck._failedTransactions.end()) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_ABORTED);
      }
      return nullptr;
    }
    
    ManagedTrx& mtrx = it->second;
    if (action == Ownership::Move) {
      if (mtrx.type == MetaType::StandaloneAQL) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                       "not allowed to directly move an AQL transaction");
      }
      
      if (mtrx.rwlock.tryWriteLock()) {
        state = mtrx.state;
        buck._managed.erase(it);
        break;
      }
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                     "transaction is in use");
    } else /*(action == Ownership::Lease)*/{
      if (AccessMode::isWriteOrExclusive(mode)) {
        if (mtrx.type == MetaType::StandaloneAQL) {
          THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                         "not allowed to write lock an AQL transaction");
        }
        if (mtrx.rwlock.tryWriteLock()) {
          mtrx.expires = defaultTTL + TRI_microtime();
          state = mtrx.state;
          break;
        }
      } else {
        if (mtrx.rwlock.tryReadLock()) {
          mtrx.expires = defaultTTL + TRI_microtime();
          state = mtrx.state;
          break;
        }
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                                       "transaction is already in use");
      }
    }
    writeLocker.unlock(); // failure;
    allTransactionsLocker.unlock();
    std::this_thread::yield();
  } while (true);
  
  if (state) {
    if (action == Ownership::Lease) {
      state->increaseNesting();
    } else {
      mode = AccessMode::Type::NONE;
    }
    return std::make_shared<ManagedContext>(tid, state, mode);
  }
  
  return nullptr;
}
  
void Manager::returnManagedTrx(TRI_voc_tid_t tid, AccessMode::Type mode) noexcept {
  const size_t bucket = getBucket(tid);

  auto& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  if (it == buck._managed.end()) {
    LOG_TOPIC(ERR, Logger::TRANSACTIONS) << "a registered transaction was not found";
    TRI_ASSERT(false);
    return;
  }
  
  if (AccessMode::isWriteOrExclusive(mode)) {
    it->second.rwlock.unlockWrite();
  } else {
    it->second.rwlock.unlockRead();
  }
}

/// @brief get the transasction state
transaction::Status Manager::getManagedTrxStatus(TRI_voc_tid_t tid) const {
  size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  
  READ_LOCKER(writeLocker, _transactions[bucket]._lock);
  
  auto const& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  if (it == buck._managed.end()) {
    // as last recourse check if this is a known failed transaction
    auto const& it2 = buck._failedTransactions.find(tid);
    if (it2 != buck._failedTransactions.end()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_TRANSACTION_ABORTED);
    }
    return transaction::Status::UNDEFINED;
  }
  
  auto const& mtrx = it->second;
  READ_LOCKER(guard, mtrx.rwlock);
  
  if (mtrx.expires > TRI_microtime()) {
    return mtrx.state->status();
  } else {
    return transaction::Status::ABORTED;
  }
}

} // namespace transaction
} // namespace arangodb
