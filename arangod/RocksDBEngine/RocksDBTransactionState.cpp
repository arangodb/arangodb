////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBTransactionState.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/system-compiler.h"
#include "Basics/system-functions.h"
#include "Cache/CacheManagerFeature.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/Counter.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "RocksDBEngine/Methods/RocksDBTrxBaseMethods.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/TransactionCollection.h"
#include "Transaction/Context.h"
#include "Transaction/Manager.h"
#include "Transaction/ManagerFeature.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <rocksdb/options.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;

/// @brief transaction type
RocksDBTransactionState::RocksDBTransactionState(
    TRI_vocbase_t& vocbase, TransactionId tid,
    transaction::Options const& options)
    : TransactionState(vocbase, tid, options),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _users(0),
#endif
      _cacheTx(nullptr) {
}

/// @brief free a transaction container
RocksDBTransactionState::~RocksDBTransactionState() {
  cleanupTransaction();
  _status = transaction::Status::ABORTED;
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
void RocksDBTransactionState::use() noexcept {
  TRI_ASSERT(_users.fetch_add(1, std::memory_order_relaxed) == 0);
}

void RocksDBTransactionState::unuse() noexcept {
  TRI_ASSERT(_users.fetch_sub(1, std::memory_order_relaxed) == 1);
}
#endif

/// @brief start a transaction
Result RocksDBTransactionState::beginTransaction(transaction::Hints hints) {
  ADB_STACK_FRAME_WITH_DATA([&](std::ostream& ss) {
    ss << AccessMode::typeString(_type) << " transaction";
  });
  LOG_TRX("0c057", TRACE, this)
      << "beginning " << AccessMode::typeString(_type) << " transaction";

  _hints = hints;  // set hints before useCollections

  auto& stats = statistics();

  Result res;
  if (isReadOnlyTransaction()) {
    // for read-only transactions there will be no locking. so we will not
    // even call TRI_microtime() to save some cycles
    res = useCollections();
  } else {
    // measure execution time of "useCollections" operation, which is
    // responsible for acquring locks as well
    double start = TRI_microtime();
    res = useCollections();

    double diff = TRI_microtime() - start;
    stats._lockTimeMicros += static_cast<uint64_t>(1000000.0 * diff);
    stats._lockTimes.count(diff);
  }

  if (res.fail()) {
    // something is wrong
    updateStatus(transaction::Status::ABORTED);
    return res;
  }

  // register with manager
  transaction::ManagerFeature::manager()->registerTransaction(
      id(), isReadOnlyTransaction(),
      hasHint(transaction::Hints::Hint::IS_FOLLOWER_TRX));
  updateStatus(transaction::Status::RUNNING);
  if (isReadOnlyTransaction()) {
    ++stats._readTransactions;
  } else {
    ++stats._transactionsStarted;
  }

  setRegistered();

  TRI_ASSERT(_cacheTx == nullptr);

  // start cache transaction
  auto* manager =
      vocbase().server().getFeature<CacheManagerFeature>().manager();
  if (manager != nullptr) {
    _cacheTx = manager->beginTransaction(isReadOnlyTransaction());
  }

  return res;
}

void RocksDBTransactionState::cleanupTransaction() noexcept {
  if (_cacheTx != nullptr) {
    // note: endTransaction() will delete _cacheTrx!
    auto* manager =
        vocbase().server().getFeature<CacheManagerFeature>().manager();
    TRI_ASSERT(manager != nullptr);
    manager->endTransaction(_cacheTx);
    _cacheTx = nullptr;

    for (auto& trxColl : _collections) {
      auto* rcoll = static_cast<RocksDBTransactionCollection*>(trxColl);
      try {
        rcoll->handleIndexCacheRefills();
      } catch (...) {
        // we have already successfully committed the transaction.
        // caches refilling should be neutral, i.e. no exceptions should
        // escape from here
      }
    }
  }
}

/// @brief commit a transaction
futures::Future<Result> RocksDBTransactionState::commitTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX("5cb03", TRACE, this)
      << "committing " << AccessMode::typeString(_type) << " transaction";

  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_ASSERT(activeTrx->isMainTransaction());
  TRI_IF_FAILURE("TransactionWriteCommitMarker") {
    return Result(TRI_ERROR_DEBUG);
  }
  auto self =
      std::static_pointer_cast<RocksDBTransactionState>(shared_from_this());
  return doCommit().thenValue([self = std::move(self), activeTrx](auto&& res) {
    if (res.ok()) {
      self->updateStatus(transaction::Status::COMMITTED);
      self->cleanupTransaction();  // deletes trx
      ++self->statistics()._transactionsCommitted;
    } else {
      // what if this fails?
      // TODO(MBkkt) What if we already committed some follower (other leaders?)
      //  And then we going to already committed transaction and ask abort it?
      //  It just doesn't work really correctly, at least because
      //  iresearch cannot abort already committed transaction.
      //  For such follower you need to create transaction with rollback
      //  operations, it same as reverting intermediate commits.
      std::ignore = self->abortTransaction(activeTrx);  // deletes trx
    }
    TRI_ASSERT(!self->_cacheTx);
    return std::forward<Result>(res);
  });
}

/// @brief abort and rollback a transaction
Result RocksDBTransactionState::abortTransaction(
    transaction::Methods* activeTrx) {
  LOG_TRX("5b226", TRACE, this)
      << "aborting " << AccessMode::typeString(_type) << " transaction";
  TRI_ASSERT(_status == transaction::Status::RUNNING);
  TRI_ASSERT(activeTrx->isMainTransaction());

  Result result = basics::catchToResult([&] { return doAbort(); });

  cleanupTransaction();  // deletes trx

  updateStatus(transaction::Status::ABORTED);
  if (hasOperations()) {
    // must clean up the query cache because the transaction
    // may have queried something via AQL that is now rolled back
    clearQueryCache();
  }

  TRI_ASSERT(!_cacheTx);
  ++statistics()._transactionsAborted;

  return result;
}

/// @brief whether or not a RocksDB iterator in this transaction must check its
/// bounds during iteration in addition to setting iterator_lower_bound or
/// iterate_upper_bound. this is currently true for all iterators that are based
/// on in-flight writes of the current transaction. it is never necessary to
/// check bounds for read-only transactions
bool RocksDBTransactionState::iteratorMustCheckBounds(
    DataSourceId cid, ReadOwnWrites readOwnWrites) const {
  return rocksdbMethods(cid)->iteratorMustCheckBounds(readOwnWrites);
}

void RocksDBTransactionState::prepareOperation(
    DataSourceId cid, RevisionId rid,
    TRI_voc_document_operation_e operationType) {
  rocksdbMethods(cid)->prepareOperation(cid, rid, operationType);
}

/// @brief add an operation for a transaction collection
Result RocksDBTransactionState::addOperation(
    DataSourceId cid, RevisionId revisionId,
    TRI_voc_document_operation_e operationType) {
  Result result = rocksdbMethods(cid)->addOperation(operationType);

  if (result.ok()) {
    auto tcoll =
        static_cast<RocksDBTransactionCollection*>(findCollection(cid));
    if (tcoll == nullptr) {
      std::string message = "collection '" + std::to_string(cid.id()) +
                            "' not found in transaction state";
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(message));
    }

    // should not fail or fail with exception
    tcoll->addOperation(operationType, revisionId);

    // clear the query cache for this collection
    auto queryCache = arangodb::aql::QueryCache::instance();

    if (queryCache->mayBeActive() && tcoll->collection()) {
      queryCache->invalidate(&_vocbase, tcoll->collection()->guid());
    }
  }

  return result;
}

RocksDBTransactionCollection::TrackedOperations&
RocksDBTransactionState::trackedOperations(DataSourceId cid) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  return static_cast<RocksDBTransactionCollection*>(col)->trackedOperations();
}

void RocksDBTransactionState::trackInsert(DataSourceId cid, RevisionId rid) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackInsert(rid);
  }
}

void RocksDBTransactionState::trackRemove(DataSourceId cid, RevisionId rid) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackRemove(rid);
  }
}

void RocksDBTransactionState::trackIndexInsert(DataSourceId cid, IndexId idxId,
                                               uint64_t hash) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexInsert(idxId,
                                                                      hash);
  }
}

void RocksDBTransactionState::trackIndexRemove(DataSourceId cid, IndexId idxId,
                                               uint64_t hash) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexRemove(idxId,
                                                                      hash);
  }
}

void RocksDBTransactionState::trackIndexCacheRefill(DataSourceId cid,
                                                    IndexId idxId,
                                                    std::string_view key) {
  auto col = findCollection(cid);
  TRI_ASSERT(col != nullptr);
  if (col != nullptr) {
    static_cast<RocksDBTransactionCollection*>(col)->trackIndexCacheRefill(
        idxId, key);
  }
}

bool RocksDBTransactionState::isOnlyExclusiveTransaction() const noexcept {
  if (!AccessMode::isWriteOrExclusive(_type)) {
    return false;
  }
  return std::none_of(_collections.begin(), _collections.end(), [](auto* coll) {
    return AccessMode::isWrite(coll->accessType());
  });
}

bool RocksDBTransactionState::hasFailedOperations() const noexcept {
  return (_status == transaction::Status::ABORTED) && hasOperations();
}

RocksDBTransactionState* RocksDBTransactionState::toState(
    transaction::Methods* trx) {
  TRI_ASSERT(trx != nullptr);
  TransactionState* state = trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(state);
}

RocksDBTransactionMethods* RocksDBTransactionState::toMethods(
    transaction::Methods* trx, DataSourceId collectionId) {
  TRI_ASSERT(trx != nullptr);
  TransactionState* state = trx->state();
  TRI_ASSERT(state != nullptr);
  return static_cast<RocksDBTransactionState*>(state)->rocksdbMethods(
      collectionId);
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
RocksDBTransactionStateGuard::RocksDBTransactionStateGuard(
    RocksDBTransactionState* state) noexcept
    : _state(state) {
  _state->use();
}

RocksDBTransactionStateGuard::~RocksDBTransactionStateGuard() {
  _state->unuse();
}
#endif

/// @brief constructor, leases a builder
RocksDBKeyLeaser::RocksDBKeyLeaser(transaction::Methods* trx)
    : _ctx(trx->transactionContextPtr()), _key(_ctx->leaseString()) {
  TRI_ASSERT(_ctx != nullptr);
  TRI_ASSERT(_key.buffer() != nullptr);
}

/// @brief destructor
RocksDBKeyLeaser::~RocksDBKeyLeaser() {
  if (!_key.usesInlineBuffer()) {
    _ctx->returnString(_key.buffer());
  }
}
