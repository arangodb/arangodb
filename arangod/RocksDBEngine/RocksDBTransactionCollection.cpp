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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBTransactionCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Exceptions.h"
#include "Basics/system-compiler.h"
#include "Logger/Logger.h"
#include "Random/RandomGenerator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "RocksDBEngine/RocksDBOptionFeature.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "Statistics/ServerStatistics.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Hints.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"

using namespace arangodb;

RocksDBTransactionCollection::RocksDBTransactionCollection(TransactionState* trx,
                                                           DataSourceId cid,
                                                           AccessMode::Type accessType)
    : TransactionCollection(trx, cid, accessType),
      _initialNumberDocuments(0),
      _revision(0),
      _numInserts(0),
      _numUpdates(0),
      _numRemoves(0),
      _usageLocked(false),
      _exclusiveWrites(
          trx->vocbase().server().getFeature<arangodb::RocksDBOptionFeature>()._exclusiveWrites) {
}

RocksDBTransactionCollection::~RocksDBTransactionCollection() = default;

/// @brief whether or not any write operations for the collection happened
bool RocksDBTransactionCollection::hasOperations() const {
  return (_numInserts > 0 || _numRemoves > 0 || _numUpdates > 0);
}

bool RocksDBTransactionCollection::canAccess(AccessMode::Type accessType) const {
  if (!_collection) {
    return false;  // not opened. probably a mistake made by the caller
  }

  // check if access type matches
  if (AccessMode::isWriteOrExclusive(accessType) &&
      !AccessMode::isWriteOrExclusive(_accessType)) {
    // type doesn't match. probably also a mistake by the caller
    return false;
  }

  return true;
}

Result RocksDBTransactionCollection::lockUsage() {
  bool doSetup = false;

  if (_collection == nullptr) {
    // open the collection
    if (!_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER) &&
        !_transaction->hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
      // use and usage-lock
      LOG_TRX("b72bb", TRACE, _transaction) << "using collection " << _cid.id();

#ifdef USE_ENTERPRISE
      // we don't need to check the permissions of collections that we only
      // read from if skipInaccessible is set
      bool checkPermissions = AccessMode::isWriteOrExclusive(_accessType) || !_transaction->options().skipInaccessibleCollections;
#else
      bool checkPermissions = true;
#endif
      // will throw if collection does not exist
      try {
        _collection = _transaction->vocbase().useCollection(_cid, checkPermissions);
      } catch (basics::Exception const& ex) {
        return {ex.code(), ex.what()};
      }

      TRI_ASSERT(_collection != nullptr);
      _usageLocked = true;
    } else {
      // use without usage-lock (lock already set externally)
      _collection = _transaction->vocbase().lookupCollection(_cid);

      if (_collection == nullptr) {
        return {TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND};
      }
    }

    doSetup = true;
  }

  TRI_ASSERT(_collection != nullptr);

  if (/*AccessMode::isWriteOrExclusive(_accessType) &&*/!isLocked()) {
    // r/w lock the collection
    Result res = doLock(_accessType);

    // TRI_ERROR_LOCKED is not an error, but it indicates that the lock
    // operation has actually acquired the lock (and that the lock has not
    // been held before)
    if (res.fail() && !res.is(TRI_ERROR_LOCKED)) {
      return res;
    }
  }

  if (doSetup) {
    RocksDBMetaCollection* rc = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
    _initialNumberDocuments = rc->meta().numberDocuments();
    _revision = rc->meta().revisionId();
  }

  return {};
}

void RocksDBTransactionCollection::releaseUsage() {
  // questionable, but seems to work
  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER) ||
      _transaction->hasHint(transaction::Hints::Hint::NO_USAGE_LOCK)) {
    TRI_ASSERT(!_usageLocked);
    _collection = nullptr;
    return;
  }

  if (isLocked()) {
    // unlock our own r/w locks
    doUnlock(_accessType);
    _lockType = AccessMode::Type::NONE;
  }

  // the top level transaction releases all collections
  if (_collection != nullptr) {
    // unuse collection, remove usage-lock
    LOG_TRX("67a6b", TRACE, _transaction) << "unusing collection " << _cid.id();

    TRI_ASSERT(_usageLocked); // simon: TODO make _usageLocked maintainer only
    if (_usageLocked) {
      _transaction->vocbase().releaseCollection(_collection.get());
      _usageLocked = false;
    }
    _collection = nullptr;
  } else {
    TRI_ASSERT(!_usageLocked);
  }
}

/// @brief add an operation for a transaction collection
void RocksDBTransactionCollection::addOperation(TRI_voc_document_operation_e operationType,
                                                RevisionId revisionId) {
  switch (operationType) {
    case TRI_VOC_DOCUMENT_OPERATION_UNKNOWN:
      break;
    case TRI_VOC_DOCUMENT_OPERATION_INSERT:
      ++_numInserts;
      _revision = revisionId;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_UPDATE:
    case TRI_VOC_DOCUMENT_OPERATION_REPLACE:
      ++_numUpdates;
      _revision = revisionId;
      break;
    case TRI_VOC_DOCUMENT_OPERATION_REMOVE:
      ++_numRemoves;
      _revision = revisionId;
      break;
  }
}

void RocksDBTransactionCollection::prepareTransaction(TransactionId trxId, uint64_t beginSeq) {
  TRI_ASSERT(_collection != nullptr);
  if (hasOperations() || !_trackedOperations.empty() || !_trackedIndexOperations.empty()) {
    auto* coll = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
    TRI_ASSERT(beginSeq > 0);
    coll->meta().placeBlocker(trxId, beginSeq);
  }
}

void RocksDBTransactionCollection::abortCommit(TransactionId trxId) {
  TRI_ASSERT(_collection != nullptr);
  if (hasOperations() || !_trackedOperations.empty() || !_trackedIndexOperations.empty()) {
    auto* coll = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
    coll->meta().removeBlocker(trxId);
  }
}

void RocksDBTransactionCollection::commitCounts(TransactionId trxId, uint64_t commitSeq) {
  TRI_IF_FAILURE("DisableCommitCounts") {
    return;
  }
  TRI_ASSERT(_collection != nullptr);
  auto* rcoll = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());

  // Update the collection count
  int64_t adj = _numInserts - _numRemoves;
  if (hasOperations()) {
    TRI_ASSERT(_revision.isSet() && commitSeq != 0);
      
    TRI_IF_FAILURE("RocksDBCommitCounts") {
      adj = 0;
    }
    TRI_IF_FAILURE("RocksDBCommitCountsRandom") {
      if (RandomGenerator::interval(uint16_t(100)) >= 50) {
        adj = 0;
      }
    }
    rcoll->meta().adjustNumberDocuments(commitSeq, _revision, adj);
  }

  // update the revision tree
  if (!_trackedOperations.empty()) {
    rcoll->bufferUpdates(commitSeq, std::move(_trackedOperations.inserts),
                         std::move(_trackedOperations.removals));
  }

  // Update the index estimates.
  for (auto& pair : _trackedIndexOperations) {
    auto idx = _collection->lookupIndex(pair.first);
    if (ADB_UNLIKELY(idx == nullptr)) {
      TRI_ASSERT(false);  // Index reported estimates, but does not exist
      continue;
    }
    auto ridx = static_cast<RocksDBIndex*>(idx.get());
    auto est = ridx->estimator();
    if (ADB_LIKELY(est != nullptr)) {
      est->bufferUpdates(commitSeq, std::move(pair.second.inserts),
                         std::move(pair.second.removals));
    } else {
      TRI_ASSERT(false);
    }
  }

  if (hasOperations() || !_trackedOperations.empty() || !_trackedIndexOperations.empty()) {
    rcoll->meta().removeBlocker(trxId);
  }

  _initialNumberDocuments += adj; // needed for intermediate commits
  _numInserts = 0;
  _numUpdates = 0;
  _numRemoves = 0;
  _trackedOperations.clear();
  _trackedIndexOperations.clear();
}

void RocksDBTransactionCollection::trackInsert(RevisionId rid) {
  if (_collection->useSyncByRevision()) {
    _trackedOperations.inserts.emplace_back(static_cast<std::uint64_t>(rid.id()));
  }
}

void RocksDBTransactionCollection::trackRemove(RevisionId rid) {
  if (_collection->useSyncByRevision()) {
    _trackedOperations.removals.emplace_back(static_cast<std::uint64_t>(rid.id()));
  }
}

void RocksDBTransactionCollection::trackIndexInsert(IndexId iid, uint64_t hash) {
  _trackedIndexOperations[iid].inserts.emplace_back(hash);
}

void RocksDBTransactionCollection::trackIndexRemove(IndexId iid, uint64_t hash) {
  _trackedIndexOperations[iid].removals.emplace_back(hash);
}

/// @brief lock a collection
/// returns TRI_ERROR_LOCKED in case the lock was successfully acquired
/// returns TRI_ERROR_NO_ERROR in case the lock does not need to be acquired and
/// no other error occurred returns any other error code otherwise
Result RocksDBTransactionCollection::doLock(AccessMode::Type type) {
  if (AccessMode::Type::WRITE == type && _exclusiveWrites) {
    type = AccessMode::Type::EXCLUSIVE;
  }
  
  if (!AccessMode::isWriteOrExclusive(type)) {
    // read operations do not require any locks in RocksDB
    _lockType = type;
    return {};
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never lock
    return {};
  }

  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(!isLocked());

  auto* physical = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  double const timeout = _transaction->lockTimeout();

  Result res;
  LOG_TRX("f1246", TRACE, _transaction) << "write-locking collection " << _cid.id();
  if (AccessMode::isExclusive(type)) {
    // exclusive locking means we'll be acquiring the collection's RW lock in
    // write mode
    res = physical->lockWrite(timeout);
  } else {
    // write locking means we'll be acquiring the collection's RW lock in read
    // mode
    res = physical->lockRead(timeout);
  }

  if (res.ok()) {
    _lockType = type;
    // not an error, but we use TRI_ERROR_LOCKED to indicate that we actually
    // acquired the lock ourselves
    res.reset(TRI_ERROR_LOCKED);
  } else if (res.is(TRI_ERROR_LOCK_TIMEOUT) && timeout >= 0.1) {
    char const* actor = _transaction->actorName();
    TRI_ASSERT(actor != nullptr);
    std::string message = "timed out after " + std::to_string(timeout) + " s waiting for "
        + AccessMode::typeString(type) + "-lock on collection "
        + _transaction->vocbase().name() + "/" + _collection->name() + " on " + actor;
    LOG_TOPIC("4512c", WARN, Logger::QUERIES) << message;
    res.reset(TRI_ERROR_LOCK_TIMEOUT, std::move(message));
      
    // increase counter for lock timeouts
    auto& stats = _transaction->statistics();
    if (AccessMode::isExclusive(type)) {
      ++stats._exclusiveLockTimeouts;
    } else {
      ++stats._writeLockTimeouts;
    }
  }

  return res;
}

/// @brief unlock a collection
Result RocksDBTransactionCollection::doUnlock(AccessMode::Type type) {
  if (AccessMode::Type::WRITE == type && _exclusiveWrites) {
    type = AccessMode::Type::EXCLUSIVE;
  }

  if (!AccessMode::isWriteOrExclusive(type) || !AccessMode::isWriteOrExclusive(_lockType)) {
    _lockType = AccessMode::Type::NONE;
    return {};
  }

  if (_transaction->hasHint(transaction::Hints::Hint::LOCK_NEVER)) {
    // never unlock
    return {};
  }

  TRI_ASSERT(_collection != nullptr);
  TRI_ASSERT(isLocked());
  
  if (!AccessMode::isWriteOrExclusive(type) && AccessMode::isWriteOrExclusive(_lockType)) {
    // do not remove a write-lock if a read-unlock was requested!
    return {};
  }
  if (AccessMode::isWriteOrExclusive(type) && !AccessMode::isWriteOrExclusive(_lockType)) {
    // we should never try to write-unlock a collection that we have only
    // read-locked
    LOG_TOPIC("2b651", ERR, arangodb::Logger::ENGINES) << "logic error in doUnlock";
    TRI_ASSERT(false);
    return {TRI_ERROR_INTERNAL, "logical error in doUnlock"};
  }

  TRI_ASSERT(_collection);

  auto* physical = static_cast<RocksDBMetaCollection*>(_collection->getPhysical());
  TRI_ASSERT(physical != nullptr);

  LOG_TRX("372c0", TRACE, _transaction) << "write-unlocking collection " << _cid.id();
  if (AccessMode::isExclusive(type)) {
    // exclusive locking means we'll be releasing the collection's RW lock in
    // write mode
    physical->unlockWrite();
  } else {
    // write locking means we'll be releasing the collection's RW lock in read
    // mode
    physical->unlockRead();
  }

  _lockType = AccessMode::Type::NONE;

  return {};
}
