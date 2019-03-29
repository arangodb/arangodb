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
  if (!_keepTransactionData) {
    return;
  }
  WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);

  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    for (auto const& it : _transactions[bucket]._activeTransactions) {
      TRI_ASSERT(it.second != nullptr);
      callback(it.first, it.second.get());
    }
  }
}

uint64_t Manager::getActiveTransactionCount() {
  return _nrRunning.load(std::memory_order_relaxed);
}

Manager::ManagedTrx::~ManagedTrx() {
  if (type == MetaType::StandaloneAQL ||
      state == nullptr ||
      state->isEmbeddedTransaction()) {
    return;
  }
  if (!state->isRunning()) {
    delete state;
    return;
  }
  transaction::Options opts;
  auto ctx = std::make_shared<transaction::ManagedContext>(2, state, AccessMode::Type::NONE);
  MGMethods trx(ctx, opts); // own state now
  trx.begin();
  TRI_ASSERT(state->nestingLevel() == 1);
  state->decreaseNesting();
  TRI_ASSERT(state->isTopLevelTransaction());
  trx.abort();
}
  
using namespace arangodb;
  
/// @brief collect forgotten transactions
bool Manager::garbageCollect(bool abortAll) {
  bool didWork = false;
  
  std::vector<TRI_voc_tid_t> gcBuffer;

  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    WRITE_LOCKER(locker, _transactions[bucket]._lock);

    double now = TRI_microtime();
    auto it = _transactions[bucket]._managed.begin();
    while (it != _transactions[bucket]._managed.end()) {
      
      ManagedTrx& mtrx = it->second;
      
      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        TRY_READ_LOCKER(tryGuard, mtrx.rwlock); // needs lock to access state
        if (!tryGuard.isLocked() || mtrx.state->isEmbeddedTransaction()) {
          // either someone has the TRX exclusively or shared
          if (abortAll) {
            mtrx.expires = 0; // soft-abort transaction
            didWork = true;
          }
        } else {
          TRI_ASSERT(mtrx.state->isRunning() && mtrx.state->isTopLevelTransaction());
          if (abortAll || mtrx.expires < now) {
            gcBuffer.emplace_back(mtrx.state->id());
          }
        }
      } else if (mtrx.type == MetaType::StandaloneAQL && mtrx.expires < now) {
        LOG_TOPIC("7ad2f", INFO, Logger::TRANSACTIONS) << "expired AQL query transaction '"
        << it->first << "'";
      } else if (mtrx.type == MetaType::Tombstone && mtrx.expires < now) {
        TRI_ASSERT(mtrx.state == nullptr);
        TRI_ASSERT(mtrx.finalStatus != transaction::Status::UNDEFINED);
        it = _transactions[bucket]._managed.erase(it);
        continue;
      }
      ++it;  // next
    }
  }
  
  for (TRI_voc_tid_t tid : gcBuffer) {
    Result res = abortManagedTrx(tid);
    if (res.fail()) {
      LOG_TOPIC("0a07f", INFO, Logger::TRANSACTIONS) << "error while GC collecting "
      "transaction: '" << res.errorMessage() << "'";
    }
    didWork = true;
  }
  if (didWork) {
    LOG_TOPIC("e5b31", INFO, Logger::TRANSACTIONS) << "collecting expired transactions";
  }
  return didWork;
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
    LOG_TOPIC("92a49", ERR, Logger::TRANSACTIONS) << "a registered transaction was not found";
    TRI_ASSERT(false);
    return;
  }
  TRI_ASSERT(it->second.type == MetaType::StandaloneAQL);
  
  /// we need to make sure no-one else is still using the TransactionState
  if (!it->second.rwlock.writeLock(/*maxAttempts*/256)) {
    LOG_TOPIC("9f7d7", ERR, Logger::TRANSACTIONS) << "a transaction is still in use";
    TRI_ASSERT(false);
    return;
  }
  buck._managed.erase(it); // unlocking not necessary
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
                     "<lockTimeout> needs to be positive");
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
  std::vector<std::string> reads, writes, exclusives;
  VPackSlice collections = trxOpts.get("collections");
  bool isValid = fillColls(collections.get("read"), reads) &&
  fillColls(collections.get("write"), writes) &&
  fillColls(collections.get("exclusive"), exclusives);
  if (!isValid) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "invalid 'collections' attribute");
  }
  
  std::unique_ptr<TransactionState> state;
  try {
    // now start our own transaction
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    state = engine->createTransactionState(vocbase, tid, options);
    TRI_ASSERT(state != nullptr);
    TRI_ASSERT(state->id() == tid);
  } catch (basics::Exception const& e) {
    return res.reset(e.code(), e.message());
  }

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
        return false;
      }
      state->addCollection(cid, cname, mode, /*nestingLevel*/0, false);
    }
    return true;
  };
  if (!lockCols(exclusives, AccessMode::Type::EXCLUSIVE) ||
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
  
  { // add transaction to bucket
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto it = _transactions[bucket]._managed.find(tid);
    if (it != _transactions[bucket]._managed.end()) {
      return res.reset(TRI_ERROR_TRANSACTION_INTERNAL, "transaction ID already used");
    }
    double expires = defaultTTL + TRI_microtime();
    TRI_ASSERT(expires > 0);
    _transactions[bucket]._managed.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(tid),
                                           std::forward_as_tuple(MetaType::Managed, state.release(),
                                                                 expires));
  }
  return res;
}

/// @brief lease the transaction, increases nesting
std::shared_ptr<transaction::Context> Manager::leaseManagedTrx(TRI_voc_tid_t tid,
                                                               AccessMode::Type mode) {
  const size_t bucket = getBucket(tid);
  
  int i = 0;
  TransactionState* state = nullptr;
  do {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    
    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end()) {
      return nullptr;
    }
    
    ManagedTrx& mtrx = it->second;
    if (mtrx.type == MetaType::Tombstone) {
      return nullptr; // already committet this trx
    }
    
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
    
    writeLocker.unlock(); // failure;
    allTransactionsLocker.unlock();
    std::this_thread::yield();
    
    if (i++ > 32) {
      LOG_TOPIC("9e972", DEBUG, Logger::TRANSACTIONS) << "waiting on trx lock " << tid;
      i = 0;
      if (application_features::ApplicationServer::isStopping()) {
        return nullptr; // shutting down
      }
    }
  } while (true);
  
  if (state) {
    state->increaseNesting();
    return std::make_shared<ManagedContext>(tid, state, mode);
  }
  TRI_ASSERT(false); // should be unreachable
  return nullptr;
}
  
void Manager::returnManagedTrx(TRI_voc_tid_t tid, AccessMode::Type mode) noexcept {
  const size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end()) {
    LOG_TOPIC("1d5b0", WARN, Logger::TRANSACTIONS) << "managed transaction was not found";
    TRI_ASSERT(false);
    return;
  }
  
  TRI_ASSERT(it->second.state != nullptr);
  TRI_ASSERT(it->second.state->isEmbeddedTransaction());
  it->second.state->decreaseNesting();
  
  // garbageCollection might soft abort transactions
  const bool isSoftAborted = it->second.expires == 0;
  if (!isSoftAborted) {
    it->second.expires = defaultTTL + TRI_microtime();
  }
  if (AccessMode::isWriteOrExclusive(mode)) {
    it->second.rwlock.unlockWrite();
  } else if (mode == AccessMode::Type::READ){
    it->second.rwlock.unlockRead();
  } else {
    TRI_ASSERT(false);
  }
      
  if (isSoftAborted) {
    abortManagedTrx(tid);
  }
}

/// @brief get the transasction state
transaction::Status Manager::getManagedTrxStatus(TRI_voc_tid_t tid) const {
  size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  READ_LOCKER(writeLocker, _transactions[bucket]._lock);
  
  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end()) {
    return transaction::Status::UNDEFINED;
  }
  
  auto const& mtrx = it->second;  
  
  if (mtrx.type == MetaType::Tombstone) {
    return mtrx.finalStatus;
  } else if (mtrx.expires > TRI_microtime() && mtrx.state != nullptr) {
    TRY_READ_LOCKER(tryGuard, mtrx.rwlock);
    if (!tryGuard.isLocked()) {
      return transaction::Status::UNDEFINED;
    }
    return mtrx.state->status();
  } else {
    return transaction::Status::ABORTED;
  }
}
  
Result Manager::commitManagedTrx(TRI_voc_tid_t tid) {
  return updateTransaction(tid, transaction::Status::COMMITTED, false);
}

Result Manager::abortManagedTrx(TRI_voc_tid_t tid) {
  return updateTransaction(tid, transaction::Status::ABORTED, false);
}

Result Manager::updateTransaction(TRI_voc_tid_t tid,
                                  transaction::Status status,
                                  bool clearServers) {
  TRI_ASSERT(status == transaction::Status::COMMITTED ||
             status == transaction::Status::ABORTED);
    
  Result res;
  const size_t bucket = getBucket(tid);
  bool wasExpired = false;
  
  std::unique_ptr<TransactionState> state;
  {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end()) {
      return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND);
    }
    
    ManagedTrx& mtrx = it->second;
    TRY_WRITE_LOCKER(tryGuard, mtrx.rwlock);
    if (!tryGuard.isLocked()) {
      return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                       "transaction is in use");
    }
    
    if (mtrx.type == MetaType::StandaloneAQL) {
      return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                       "not allowed to change an AQL transaction");
    } else if (mtrx.type == MetaType::Tombstone) {
      TRI_ASSERT(mtrx.state == nullptr);
      // make sure everyone who asks gets the updated timestamp
      mtrx.expires = TRI_microtime() + tombstoneTTL;
      if (mtrx.finalStatus == status) {
        return res; // all good
      } else {
        return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                         "transaction was already committed / aborted");
      }
    }
    
    double now = TRI_microtime();
    if (mtrx.expires < now) {
      status = transaction::Status::ABORTED;
      wasExpired = true;
    }
    
    state.reset(mtrx.state);
    mtrx.state = nullptr;
    mtrx.type = MetaType::Tombstone;
    mtrx.expires = now + tombstoneTTL;
    mtrx.finalStatus = status;
    // it is sufficient to pretend that the operation already succeeded
  }
  
  TRI_ASSERT(state);
  if (!state) { // this should never happen
    return res.reset(TRI_ERROR_INTERNAL, "managed trx in an invalid state");
  }
  
  auto abortTombstone = [&] {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      it->second.finalStatus = Status::ABORTED;
    }
  };
  if (!state->isRunning()) { // this also should not happen
    abortTombstone();
    return res.reset(TRI_ERROR_TRANSACTION_ABORTED, "transaction was not running");
  }
  
  auto ctx = std::make_shared<ManagedContext>(tid, state.get(), AccessMode::Type::NONE);
  state.release(); // now owned by ctx
  
  transaction::Options trxOpts;
  MGMethods trx(ctx, trxOpts);
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.state()->nestingLevel() == 1);
  trx.state()->decreaseNesting();
  TRI_ASSERT(trx.state()->isTopLevelTransaction());
  if (clearServers) {
    state->clearKnownServers();
  }
  if (status == transaction::Status::COMMITTED) {
    res = trx.commit();
    if (res.fail()) { // set final status to aborted
      abortTombstone();
    }
  } else {
    res = trx.abort();
    if (res.ok() && wasExpired) {
      res.reset(TRI_ERROR_TRANSACTION_ABORTED);
    }
    TRI_ASSERT(!trx.state()->isRunning());
  }
  
  return res;
}
  
/// @brief abort all transactions on a given shard
void Manager::abortAllManagedTrx(TRI_voc_cid_t cid, bool leader) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  std::vector<TRI_voc_tid_t> toAbort;
  
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);
    
    auto it = _transactions[bucket]._managed.begin();
    while (it != _transactions[bucket]._managed.end()) {
      
      ManagedTrx& mtrx = it->second;
      
      // abort matching leader / follower transaction
      if (mtrx.type == MetaType::Managed &&
          ((leader && isLeaderTransactionId(it->first)) ||
           (!leader && isFollowerTransactionId(it->first)) )) {
        TRI_ASSERT(mtrx.state != nullptr);
        TRY_READ_LOCKER(tryGuard, mtrx.rwlock); // needs lock to access state
        if (tryGuard.isLocked()) {
          TransactionCollection* tcoll = mtrx.state->collection(cid, AccessMode::Type::NONE);
          if (tcoll != nullptr) {
            toAbort.emplace_back(it->first);
          }
        }
      }
      ++it;  // next
    }
  }
  
  for (TRI_voc_tid_t tid : toAbort) {
    Result res = updateTransaction(tid, Status::ABORTED, /*clearSrvs*/true);
    if (res.fail()) {
      LOG_TOPIC("2bf48", INFO, Logger::TRANSACTIONS) << "error while GC collecting "
      "transaction: '" << res.errorMessage() << "'";
    }
  }
}
  
#ifdef ARANGODB_USE_CATCH_TESTS
Manager::TrxCounts Manager::getManagedTrxCount() const {
  TrxCounts counts;
  
  WRITE_LOCKER(allTransactionsLocker, _allTransactionsLock);
  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);
    for (auto const& pair : _transactions[bucket]._managed) {
      if (pair.second.type != MetaType::Managed) {
        counts.numManaged++;
      } else if (pair.second.type == MetaType::StandaloneAQL) {
        counts.numStandaloneAQL++;
      } else if (pair.second.type == MetaType::Tombstone) {
        counts.numTombstones++;
      }
    }
  }
  return counts;
}
#endif

}  // namespace transaction
}  // namespace arangodb
