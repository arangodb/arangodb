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
#include "Basics/system-functions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Futures/Utilities.h"
#include "GeneralServer/AuthenticationFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/SmartContext.h"
#include "Transaction/Status.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/ExecContext.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/VirtualCollection.h"
#endif

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <thread>

namespace {
bool authorized(std::string const& user) {
  auto const& exec = arangodb::ExecContext::current();
  if (exec.isSuperuser()) {
    return true;
  }
  return (user == exec.user());
}

std::string currentUser() {
  return arangodb::ExecContext::current().user();
}
}  // namespace

namespace arangodb {
namespace transaction {

const size_t Manager::maxTransactionSize;  // 128 MiB

namespace {
struct MGMethods final : arangodb::transaction::Methods {
  MGMethods(std::shared_ptr<arangodb::transaction::Context> const& ctx,
            arangodb::transaction::Options const& opts)
      : Methods(ctx, opts) {
    TRI_ASSERT(_state->isEmbeddedTransaction());
  }
};
}  // namespace

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
                                  std::unique_ptr<TransactionData> data,
                                  bool isReadOnlyTransaction) {
  if (!isReadOnlyTransaction) {
    _rwLock.readLock();
  }

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
void Manager::unregisterTransaction(TRI_voc_tid_t transactionId, bool markAsFailed,
                                    bool isReadOnlyTransaction) {
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
  if (!isReadOnlyTransaction) {
    _rwLock.unlockRead();
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

Manager::ManagedTrx::ManagedTrx(MetaType t, TransactionState* st)
    : type(t),
      finalStatus(Status::UNDEFINED),
      usedTimeSecs(TRI_microtime()),
      state(st),
      user(::currentUser()),
      rwlock() {}

bool Manager::ManagedTrx::expired() const {
  double now = TRI_microtime();
  if (type == Manager::MetaType::Tombstone) {
    return (now - usedTimeSecs) > tombstoneTTL;
  }

  auto role = ServerState::instance()->getRole();
  if ((ServerState::isSingleServer(role) || ServerState::isCoordinator(role))) {
    return (now - usedTimeSecs) > idleTTL;
  }
  return (now - usedTimeSecs) > idleTTLDBServer;
}

Manager::ManagedTrx::~ManagedTrx() {
  if (type == MetaType::StandaloneAQL || state == nullptr || state->isEmbeddedTransaction()) {
    return;  // not managed by us
  }
  if (!state->isRunning()) {
    delete state;
    return;
  }

  try {
    transaction::Options opts;
    auto ctx =
        std::make_shared<transaction::ManagedContext>(2, state, AccessMode::Type::NONE);
    MGMethods trx(ctx, opts);  // own state now
    Result res = trx.begin();
    (void) res;
    TRI_ASSERT(state->nestingLevel() == 1);
    state->decreaseNesting();
    TRI_ASSERT(state->isTopLevelTransaction());
    trx.abort();
  } catch (...) {
    // obviously it is not good to consume all exceptions here,
    // but we are in a destructor and must never throw from here
  }
}

using namespace arangodb;

/// @brief register a transaction shard
/// @brief tid global transaction shard
/// @param cid the optional transaction ID (use 0 for a single shard trx)
void Manager::registerAQLTrx(TransactionState* state) {
  if (_disallowInserts.load(std::memory_order_acquire)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
  }

  TRI_ASSERT(state != nullptr);
  auto const tid = state->id();
  size_t const bucket = getBucket(tid);
  {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_TRANSACTION_INTERNAL,
                                     std::string("transaction ID ") + std::to_string(tid) + "' already used in registerAQLTrx");
    }
    buck._managed.emplace(std::piecewise_construct, std::forward_as_tuple(tid),
                          std::forward_as_tuple(MetaType::StandaloneAQL, state));
  }
}

void Manager::unregisterAQLTrx(TRI_voc_tid_t tid) noexcept {
  const size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto& buck = _transactions[bucket];
  auto it = buck._managed.find(tid);
  if (it == buck._managed.end()) {
    LOG_TOPIC("92a49", ERR, Logger::TRANSACTIONS)
        << "a registered transaction was not found";
    TRI_ASSERT(false);
    return;
  }
  TRI_ASSERT(it->second.type == MetaType::StandaloneAQL);

  /// we need to make sure no-one else is still using the TransactionState
  if (!it->second.rwlock.writeLock(/*maxAttempts*/ 256)) {
    LOG_TOPIC("9f7d7", ERR, Logger::TRANSACTIONS)
        << "a transaction is still in use";
    TRI_ASSERT(false);
    return;
  }

  buck._managed.erase(it);  // unlocking not necessary
}

Result Manager::createManagedTrx(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                                 VPackSlice const trxOpts) {
  Result res;
  if (_disallowInserts) {
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  // parse the collections to register
  if (!trxOpts.isObject() || !trxOpts.get("collections").isObject()) {
    return res.reset(TRI_ERROR_BAD_PARAMETER, "missing 'collections'");
  }

  // extract the properties from the object
  transaction::Options options;
  options.fromVelocyPack(trxOpts);
  if (options.lockTimeout < 0.0) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "<lockTimeout> needs to be positive");
  }

  auto fillColls = [](VPackSlice const& slice, std::vector<std::string>& cols) {
    if (slice.isNone()) {  // ignore nonexistent keys
      return true;
    } else if (slice.isString()) {
      cols.emplace_back(slice.copyString());
      return true;
    }

    if (slice.isArray()) {
      for (VPackSlice val : VPackArrayIterator(slice)) {
        if (!val.isString() || val.getStringLength() == 0) {
          return false;
        }
        cols.emplace_back(val.copyString());
      }
      return true;
    }
    return false;
  };
  std::vector<std::string> reads, writes, exclusives;
  VPackSlice collections = trxOpts.get("collections");
  bool isValid = fillColls(collections.get("read"), reads) &&
                 fillColls(collections.get("write"), writes) &&
                 fillColls(collections.get("exclusive"), exclusives);
  if (!isValid) {
    return res.reset(TRI_ERROR_BAD_PARAMETER,
                     "invalid 'collections' attribute");
  }

  return createManagedTrx(vocbase, tid, reads, writes, exclusives, std::move(options));
}

/// @brief create managed transaction
Result Manager::createManagedTrx(TRI_vocbase_t& vocbase, TRI_voc_tid_t tid,
                                 std::vector<std::string> const& readCollections,
                                 std::vector<std::string> const& writeCollections,
                                 std::vector<std::string> const& exclusiveCollections,
                                 transaction::Options options) {
  Result res;
  if (_disallowInserts.load(std::memory_order_acquire)) {
    return res.reset(TRI_ERROR_SHUTTING_DOWN);
  }

  const size_t bucket = getBucket(tid);

  {  // quick check whether ID exists
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                       std::string("transaction ID '") + std::to_string(tid) + "' already used in createManagedTrx lookup");
    }
  }

  // enforce size limit per DBServer
  options.maxTransactionSize =
      std::min<size_t>(options.maxTransactionSize, Manager::maxTransactionSize);

  std::unique_ptr<TransactionState> state;
  try {
    // now start our own transaction
    StorageEngine* engine = EngineSelectorFeature::ENGINE;
    state = engine->createTransactionState(vocbase, tid, options);
  } catch (basics::Exception const& e) {
    return res.reset(e.code(), e.message());
  }
  TRI_ASSERT(state != nullptr);
  TRI_ASSERT(state->id() == tid);

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
        // not found
        res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                  std::string(TRI_errno_string(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND)) +
                      ":" + cname);
      } else {
#ifdef USE_ENTERPRISE
        if (state->isCoordinator()) {
          std::shared_ptr<LogicalCollection> col = resolver.getCollection(cname);
          if (col->isSmart() && col->type() == TRI_COL_TYPE_EDGE) {
            auto theEdge = dynamic_cast<arangodb::VirtualSmartEdgeCollection*>(col.get());
            if (theEdge == nullptr) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "cannot cast collection to smart edge collection");
            }
            res.reset(state->addCollection(theEdge->getLocalCid(), "_local_" + cname, mode, /*nestingLevel*/ 0, false));
            if (res.fail()) {
              return false;
            }
            res.reset(state->addCollection(theEdge->getFromCid(), "_from_" + cname, mode, /*nestingLevel*/ 0, false));
            if (res.fail()) {
              return false;
            }
            res.reset(state->addCollection(theEdge->getToCid(), "_to_" + cname, mode, /*nestingLevel*/ 0, false));
            if (res.fail()) {
              return false;
            }
          }
        }
#endif
        res.reset(state->addCollection(cid, cname, mode, /*nestingLevel*/ 0, false));
      }

      if (res.fail()) {
        return false;
      }
    }
    return true;
  };
  if (!lockCols(exclusiveCollections, AccessMode::Type::EXCLUSIVE) ||
      !lockCols(writeCollections, AccessMode::Type::WRITE) ||
      !lockCols(readCollections, AccessMode::Type::READ)) {
    if (res.fail()) {
      // error already set by callback function
      return res;
    }
    // no error set. so it must be "data source not found"
    return res.reset(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // start the transaction
  transaction::Hints hints;
  hints.set(transaction::Hints::Hint::LOCK_ENTIRELY);
  hints.set(transaction::Hints::Hint::GLOBAL_MANAGED);
  res = state->beginTransaction(hints);  // registers with transaction manager
  if (res.fail()) {
    TRI_ASSERT(!state->isRunning());
    return res;
  }

  {  // add transaction to bucket
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto it = _transactions[bucket]._managed.find(tid);
    if (it != _transactions[bucket]._managed.end()) {
      return res.reset(TRI_ERROR_TRANSACTION_INTERNAL,
                       std::string("transaction ID '") + std::to_string(tid) + "' already used in createManagedTrx insert");
    }
    TRI_ASSERT(state->id() == tid);
    _transactions[bucket]._managed.emplace(std::piecewise_construct,
                                           std::forward_as_tuple(tid),
                                           std::forward_as_tuple(MetaType::Managed,
                                                                 state.release()));
  }

  LOG_TOPIC("d6806", DEBUG, Logger::TRANSACTIONS) << "created managed trx '" << tid << "'";

  return res;
}

/// @brief lease the transaction, increases nesting
std::shared_ptr<transaction::Context> Manager::leaseManagedTrx(TRI_voc_tid_t tid,
                                                               AccessMode::Type mode) {
  auto& server = application_features::ApplicationServer::server();
  if (_disallowInserts.load(std::memory_order_acquire)) {
    return nullptr;
  }
  
  size_t const bucket = getBucket(tid);
  int i = 0;
  TransactionState* state = nullptr;
  do {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.find(tid);
    if (it == _transactions[bucket]._managed.end() || !::authorized(it->second.user)) {
      return nullptr;
    }

    ManagedTrx& mtrx = it->second;
    if (mtrx.type == MetaType::Tombstone) {
      return nullptr;  // already committed this trx
    }

    if (AccessMode::isWriteOrExclusive(mode)) {
      if (mtrx.type == MetaType::StandaloneAQL) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
            "not allowed to write lock an AQL transaction");
      }
      if (mtrx.rwlock.tryWriteLock()) {
        state = mtrx.state;
        break;
      }
    } else {
      if (mtrx.rwlock.tryReadLock()) {
        state = mtrx.state;
        break;
      }

      LOG_TOPIC("abd72", DEBUG, Logger::TRANSACTIONS) << "transaction '" << tid << "' is already in use";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_LOCKED,
                                     std::string("transaction '") + std::to_string(tid) + "' is already in use");
    }

    writeLocker.unlock();  // failure;
    allTransactionsLocker.unlock();
    std::this_thread::yield();

    if (i++ > 32) {
      LOG_TOPIC("9e972", DEBUG, Logger::TRANSACTIONS) << "waiting on trx lock " << tid;
      i = 0;
      if (server.isStopping()) {
        return nullptr;  // shutting down
      }
    }
  } while (true);

  if (state) {
    int level = state->increaseNesting();
    TRI_ASSERT(!AccessMode::isWriteOrExclusive(mode) || level == 1);
    return std::make_shared<ManagedContext>(tid, state, mode);
  }
  TRI_ASSERT(false);  // should be unreachable
  return nullptr;
}

void Manager::returnManagedTrx(TRI_voc_tid_t tid, AccessMode::Type mode) noexcept {
  const size_t bucket = getBucket(tid);
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

  auto it = _transactions[bucket]._managed.find(tid);
  if (it == _transactions[bucket]._managed.end() || !::authorized(it->second.user)) {
    LOG_TOPIC("1d5b0", WARN, Logger::TRANSACTIONS)
        << "managed transaction was not found";
    TRI_ASSERT(false);
    return;
  }

  TRI_ASSERT(it->second.state != nullptr);
  TRI_ASSERT(it->second.state->isEmbeddedTransaction());
  int level = it->second.state->decreaseNesting();
  TRI_ASSERT(!AccessMode::isWriteOrExclusive(mode) || level == 0);

  // garbageCollection might soft abort used transactions
  const bool isSoftAborted = it->second.usedTimeSecs == 0;
  if (!isSoftAborted) {
    it->second.usedTimeSecs = TRI_microtime();
  }
  if (AccessMode::isWriteOrExclusive(mode)) {
    it->second.rwlock.unlockWrite();
  } else if (mode == AccessMode::Type::READ) {
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
  if (it == _transactions[bucket]._managed.end() || !::authorized(it->second.user)) {
    return transaction::Status::UNDEFINED;
  }

  auto const& mtrx = it->second;

  if (mtrx.type == MetaType::Tombstone) {
    return mtrx.finalStatus;
  } else if (!mtrx.expired() && mtrx.state != nullptr) {
    return transaction::Status::RUNNING;
  } else {
    return transaction::Status::ABORTED;
  }
}
  

Result Manager::statusChangeWithTimeout(TRI_voc_tid_t tid, transaction::Status status) {
  double startTime = 0.0;
  constexpr double maxWaitTime = 2.0;
  Result res;
  while (true) {
    res = updateTransaction(tid, status, false);
    if (res.ok() || !res.is(TRI_ERROR_LOCKED)) {
      break;
    }
    if (startTime <= 0.0001) { // fp tolerance
      startTime = TRI_microtime();
    } else if (TRI_microtime() - startTime > maxWaitTime) {
      // timeout
      break;
    }
    std::this_thread::yield();
  }
  return res;
}

Result Manager::commitManagedTrx(TRI_voc_tid_t tid) {
  return statusChangeWithTimeout(tid, transaction::Status::COMMITTED);
}

Result Manager::abortManagedTrx(TRI_voc_tid_t tid) {
  return statusChangeWithTimeout(tid, transaction::Status::ABORTED);
}

Result Manager::updateTransaction(TRI_voc_tid_t tid, transaction::Status status,
                                  bool clearServers) {
  TRI_ASSERT(status == transaction::Status::COMMITTED ||
             status == transaction::Status::ABORTED);

  LOG_TOPIC("7bd2f", DEBUG, Logger::TRANSACTIONS)
      << "managed trx '" << tid << " updating to '" << status << "'";

  Result res;
  size_t const bucket = getBucket(tid);
  bool wasExpired = false;

  std::unique_ptr<TransactionState> state;
  {
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it == buck._managed.end() || !::authorized(it->second.user)) {
      return res.reset(TRI_ERROR_TRANSACTION_NOT_FOUND, std::string("transaction '") + std::to_string(tid) + "' not found");
    }

    ManagedTrx& mtrx = it->second;
    TRY_WRITE_LOCKER(tryGuard, mtrx.rwlock);
    if (!tryGuard.isLocked()) {
      LOG_TOPIC("dfc30", DEBUG, Logger::TRANSACTIONS) << "transaction '" << tid << "' is in use";
      return res.reset(TRI_ERROR_LOCKED,
                       std::string("transaction '") + std::to_string(tid) + "' is in use");
    }
    TRI_ASSERT(tryGuard.isLocked());

    if (mtrx.type == MetaType::StandaloneAQL) {
      return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION,
                       "not allowed to change an AQL transaction");
    } else if (mtrx.type == MetaType::Tombstone) {
      TRI_ASSERT(mtrx.state == nullptr);
      // make sure everyone who asks gets the updated timestamp
      mtrx.usedTimeSecs = TRI_microtime();
      if (mtrx.finalStatus == status) {
        return res;  // all good
      } else {
        std::string msg("transaction was already ");
        msg.append(statusString(mtrx.finalStatus));
        return res.reset(TRI_ERROR_TRANSACTION_DISALLOWED_OPERATION, std::move(msg));
      }
    }

    if (mtrx.expired() && status != transaction::Status::ABORTED) {
      status = transaction::Status::ABORTED;
      wasExpired = true;
    }

    state.reset(mtrx.state);
    mtrx.state = nullptr;
    mtrx.type = MetaType::Tombstone;
    mtrx.usedTimeSecs = TRI_microtime();
    mtrx.finalStatus = status;
    // it is sufficient to pretend that the operation already succeeded
  }

  TRI_ASSERT(state);
  if (!state) {  // this should never happen
    return res.reset(TRI_ERROR_INTERNAL, "managed trx in an invalid state");
  }

  auto abortTombstone = [&] {  // set tombstone entry to aborted
    READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
    WRITE_LOCKER(writeLocker, _transactions[bucket]._lock);
    auto& buck = _transactions[bucket];
    auto it = buck._managed.find(tid);
    if (it != buck._managed.end()) {
      it->second.finalStatus = Status::ABORTED;
    }
  };
  if (!state->isRunning()) {  // this also should not happen
    abortTombstone();
    return res.reset(TRI_ERROR_TRANSACTION_ABORTED,
                     "transaction was not running");
  }

  bool const isCoordinator = state->isCoordinator();

  auto ctx = std::make_shared<ManagedContext>(tid, state.get(), AccessMode::Type::NONE);
  state.release();  // now owned by ctx

  transaction::Options trxOpts;
  MGMethods trx(ctx, trxOpts);
  TRI_ASSERT(trx.state()->isRunning());
  TRI_ASSERT(trx.state()->nestingLevel() == 1);
  trx.state()->decreaseNesting();
  TRI_ASSERT(trx.state()->isTopLevelTransaction());
  if (clearServers && !isCoordinator) {
    trx.state()->clearKnownServers();
  }
  if (status == transaction::Status::COMMITTED) {
    res = trx.commit();
    if (res.fail()) {  // set final status to aborted
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

/// @brief calls the callback function for each managed transaction
void Manager::iterateManagedTrx(std::function<void(TRI_voc_tid_t, ManagedTrx const&)> const& callback) const {
  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  // iterate over all active transactions
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto& buck = _transactions[bucket];
    for (auto const& it : buck._managed) {
      if (it.second.type == MetaType::Managed) {
        // we only care about managed transactions here
        callback(it.first, it.second);
      }
    }
  }
}

/// @brief collect forgotten transactions
bool Manager::garbageCollect(bool abortAll) {
  bool didWork = false;
  SmallVector<TRI_voc_tid_t, 64>::allocator_type::arena_type arena;
  SmallVector<TRI_voc_tid_t, 64> toAbort{arena};

  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);

  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    WRITE_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.begin();
    while (it != _transactions[bucket]._managed.end()) {
      ManagedTrx& mtrx = it->second;
      
      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        if (abortAll || mtrx.expired()) {
          TRY_READ_LOCKER(tryGuard, mtrx.rwlock);  // needs lock to access state

          if (tryGuard.isLocked()) {
            TRI_ASSERT(mtrx.state->isRunning() && mtrx.state->isTopLevelTransaction());
            TRI_ASSERT(it->first == mtrx.state->id());
            toAbort.emplace_back(mtrx.state->id());
          } else if (abortAll) {    // transaction is in
            mtrx.usedTimeSecs = 0;  // soft-abort transaction
            didWork = true;
          }
        }
      } else if (mtrx.type == MetaType::StandaloneAQL && mtrx.expired()) {
        LOG_TOPIC("7ad3f", INFO, Logger::TRANSACTIONS)
            << "expired AQL query transaction '" << it->first << "'";
      } else if (mtrx.type == MetaType::Tombstone && mtrx.expired()) {
        TRI_ASSERT(mtrx.state == nullptr);
        TRI_ASSERT(mtrx.finalStatus != transaction::Status::UNDEFINED);
        it = _transactions[bucket]._managed.erase(it);
        continue;
      }

      ++it;  // next
    }
  }

  for (TRI_voc_tid_t tid : toAbort) {
    LOG_TOPIC("6fbaf", INFO, Logger::TRANSACTIONS) << "garbage collecting "
                                                   << "transaction: '" << tid << "'";
    Result res = updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true);
    // updateTransaction can return TRI_ERROR_TRANSACTION_ABORTED when it
    // successfully aborts, so ignore this error.
    // we can also get the TRI_ERROR_LOCKED error in case we cannot
    // immediately acquire the lock on the transaction. this _can_ happen
    // infrequently, but is not an error
    if (res.fail() && 
        !res.is(TRI_ERROR_TRANSACTION_ABORTED) &&
        !res.is(TRI_ERROR_LOCKED)) {
      LOG_TOPIC("0a07f", INFO, Logger::TRANSACTIONS) << "error while aborting "
                                                        "transaction: '"
                                                     << res.errorMessage() << "'";
    }
    didWork = true;
  }

  if (didWork) {
    LOG_TOPIC("e5b31", INFO, Logger::TRANSACTIONS)
        << "aborted expired transactions";
  }

  return didWork;
}

/// @brief abort all transactions matching
bool Manager::abortManagedTrx(std::function<bool(TransactionState const&)> cb) {
  SmallVector<TRI_voc_tid_t, 64>::allocator_type::arena_type arena;
  SmallVector<TRI_voc_tid_t, 64> toAbort{arena};

  READ_LOCKER(allTransactionsLocker, _allTransactionsLock);
  for (size_t bucket = 0; bucket < numBuckets; ++bucket) {
    READ_LOCKER(locker, _transactions[bucket]._lock);

    auto it = _transactions[bucket]._managed.begin();
    while (it != _transactions[bucket]._managed.end()) {
      ManagedTrx& mtrx = it->second;
      if (mtrx.type == MetaType::Managed) {
        TRI_ASSERT(mtrx.state != nullptr);
        TRY_READ_LOCKER(tryGuard, mtrx.rwlock);  // needs lock to access state
        if (tryGuard.isLocked() && cb(*mtrx.state)) {
          toAbort.emplace_back(it->first);
        }
      }

      ++it;  // next
    }
  }

  for (TRI_voc_tid_t tid : toAbort) {
    Result res = updateTransaction(tid, Status::ABORTED, /*clearSrvs*/ true);
    if (res.fail()) {
      LOG_TOPIC("2bf48", INFO, Logger::TRANSACTIONS) << "error aborting "
                                                        "transaction: '"
                                                     << res.errorMessage() << "'";
    }
  }
  return !toAbort.empty();
}

void Manager::toVelocyPack(VPackBuilder& builder, std::string const& database,
                           std::string const& username, bool fanout) const {
  TRI_ASSERT(!builder.isClosed());

  if (fanout) {
    TRI_ASSERT(ServerState::instance()->isCoordinator());
    auto& ci = _feature.server().getFeature<ClusterFeature>().clusterInfo();

    NetworkFeature const& nf = _feature.server().getFeature<NetworkFeature>();
    network::ConnectionPool* pool = nf.pool();
    if (pool == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_SHUTTING_DOWN);
    }

    std::vector<network::FutureRes> futures;
    auto auth = AuthenticationFeature::instance();

    network::RequestOptions options;
    options.timeout = network::Timeout(30.0);

    VPackBuffer<uint8_t> body;

    for (auto const& coordinator : ci.getCurrentCoordinators()) {
      if (coordinator == ServerState::instance()->getId()) {
        // ourselves!
        continue;
      }

      network::Headers headers;
      if (auth != nullptr && auth->isActive()) {
        if (!username.empty()) {
          VPackBuilder builder;
          {
            VPackObjectBuilder payload{&builder};
            payload->add("preferred_username", VPackValue(username));
          }
          VPackSlice slice = builder.slice();
          headers.emplace(StaticStrings::Authorization,
                          "bearer " + auth->tokenCache().generateJwt(slice));
        } else {
          headers.emplace(StaticStrings::Authorization,
                          "bearer " + auth->tokenCache().jwtToken());
        }
      }

      auto f = network::sendRequest(pool, "server:" + coordinator, fuerte::RestVerb::Get,
                                    "/_db/" + database +
                                        "/_api/transaction?local=true",
                                    body, std::move(headers), options);
      futures.emplace_back(std::move(f));
    }

    if (!futures.empty()) {
      auto responses = futures::collectAll(futures).get();
      for (auto const& it : responses) {
        if (!it.hasValue()) {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_BACKEND_UNAVAILABLE);
        }
        auto& res = it.get();
        if (res.response && res.response->statusCode() == fuerte::StatusOK) {
          auto slices = res.response->slices();
          if (!slices.empty()) {
            VPackSlice slice = slices[0];
            if (slice.isObject()) {
              slice = slice.get("transactions");
              if (slice.isArray()) {
                for (auto const& it : VPackArrayIterator(slice)) {
                  builder.add(it);
                }
              }
            }
          }
        }
      }
    }
  }

  // merge with local transactions
  iterateManagedTrx([&builder](TRI_voc_tid_t tid, ManagedTrx const& trx) {
    if (::authorized(trx.user)) {
      builder.openObject(true);
      builder.add("id", VPackValue(std::to_string(tid)));
      builder.add("state", VPackValue(transaction::statusString(trx.state->status())));
      builder.close();
    }
  });
}

}  // namespace transaction
}  // namespace arangodb
