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

#include "RocksDBBuilderIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/application-exit.h"
#include "Containers/HashSet.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/comparator.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

namespace {
struct BuilderTrx : public arangodb::transaction::Methods {
  BuilderTrx(std::shared_ptr<transaction::Context> const& transactionContext,
             LogicalDataSource const& collection, AccessMode::Type type)
      : transaction::Methods(transactionContext), _cid(collection.id()) {
    // add the (sole) data-source
    addCollection(collection.id(), collection.name(), type);
    addHint(transaction::Hints::Hint::NO_DLD);
  }

  /// @brief get the underlying transaction collection
  RocksDBTransactionCollection* resolveTrxCollection() {
    return static_cast<RocksDBTransactionCollection*>(trxCollection(_cid));
  }

 private:
  TRI_voc_cid_t _cid;
};

struct BuilderCookie : public arangodb::TransactionState::Cookie {
  // do not track removed documents twice
  ::arangodb::containers::HashSet<LocalDocumentId::BaseType> tracked;
};
}  // namespace

RocksDBBuilderIndex::RocksDBBuilderIndex(std::shared_ptr<arangodb::RocksDBIndex> const& wp)
    : RocksDBIndex(wp->id(), wp->collection(), wp->name(), wp->fields(),
                   wp->unique(), wp->sparse(), wp->columnFamily(),
                   wp->objectId(), wp->tempObjectId(), /*useCache*/ false),
      _wrapped(wp) {
  TRI_ASSERT(_wrapped);
}

/// @brief return a VelocyPack representation of the index
void RocksDBBuilderIndex::toVelocyPack(VPackBuilder& builder,
                                       std::underlying_type<Serialize>::type flags) const {
  VPackBuilder inner;
  _wrapped->toVelocyPack(inner, flags);
  TRI_ASSERT(inner.slice().isObject());
  builder.openObject();  // FIXME refactor RocksDBIndex::toVelocyPack !!
  builder.add(velocypack::ObjectIterator(inner.slice()));
  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    builder.add("_inprogress", VPackValue(true));
  }
  builder.close();
}

/// insert index elements into the specified write batch.
Result RocksDBBuilderIndex::insert(transaction::Methods& trx, RocksDBMethods* mthd,
                                   LocalDocumentId const& documentId,
                                   arangodb::velocypack::Slice const& slice,
                                   OperationOptions& options) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<::BuilderCookie*>(trx.state()->cookie(this));
#else
  auto* ctx = static_cast<::BuilderCookie*>(trx.state()->cookie(this));
#endif
  if (!ctx) {
    auto ptr = std::make_unique<::BuilderCookie>();
    ctx = ptr.get();
    trx.state()->cookie(this, std::move(ptr));
  }

  // do not track document more than once
  if (ctx->tracked.find(documentId.id()) == ctx->tracked.end()) {
    ctx->tracked.insert(documentId.id());
    RocksDBLogValue val = RocksDBLogValue::TrackedDocumentInsert(documentId, slice);
    mthd->PutLogData(val.slice());
  }
  return Result();  // do nothing
}

/// remove index elements and put it in the specified write batch.
Result RocksDBBuilderIndex::remove(transaction::Methods& trx, RocksDBMethods* mthd,
                                   LocalDocumentId const& documentId,
                                   arangodb::velocypack::Slice const& slice,
                                   OperationMode mode) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  auto* ctx = dynamic_cast<::BuilderCookie*>(trx.state()->cookie(this));
#else
  auto* ctx = static_cast<::BuilderCookie*>(trx.state()->cookie(this));
#endif
  if (!ctx) {
    auto ptr = std::make_unique<::BuilderCookie>();
    ctx = ptr.get();
    trx.state()->cookie(this, std::move(ptr));
  }

  // do not track document more than once
  if (ctx->tracked.find(documentId.id()) == ctx->tracked.end()) {
    ctx->tracked.insert(documentId.id());
    RocksDBLogValue val = RocksDBLogValue::TrackedDocumentRemove(documentId, slice);
    mthd->PutLogData(val.slice());
  }
  return Result();  // do nothing
}

// fast mode assuming exclusive access locked from outside
template <typename WriteBatchType, typename MethodsType, bool foreground>
static arangodb::Result fillIndex(RocksDBIndex& ridx, WriteBatchType& batch,
                                  rocksdb::Snapshot const* snap) {
  // fillindex can be non transactional, we just need to clean up
  rocksdb::DB* rootDB = rocksutils::globalRocksDB()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);

  RocksDBCollection* rcoll =
      static_cast<RocksDBCollection*>(ridx.collection().getPhysical());
  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  rocksdb::Slice upper(bounds.end());

  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false;  // TODO set to true eventually

  rocksdb::ReadOptions ro(/*cksum*/ false, /*cache*/ false);
  ro.snapshot = snap;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;

  rocksdb::ColumnFamilyHandle* docCF = RocksDBColumnFamily::documents();
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));

  auto mode = snap == nullptr ? AccessMode::Type::EXCLUSIVE : AccessMode::Type::WRITE;
  LogicalCollection& coll = ridx.collection();
  ::BuilderTrx trx(transaction::StandaloneContext::Create(coll.vocbase()), coll, mode);
  if (mode == AccessMode::Type::EXCLUSIVE) {
    trx.addHint(transaction::Hints::Hint::LOCK_NEVER);
  }
  trx.addHint(transaction::Hints::Hint::INDEX_CREATION);
  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  TRI_IF_FAILURE("RocksDBBuilderIndex::fillIndex") { FATAL_ERROR_EXIT(); }

  uint64_t numDocsWritten = 0;
  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBTransactionCollection* trxColl = trx.resolveTrxCollection();
  // write batch will be reset every x documents
  MethodsType batched(state, &batch);
  
  auto commitLambda = [&] {
    if (batch.GetWriteBatch()->Count() > 0) {
      s = rootDB->Write(wo, batch.GetWriteBatch());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      }
    }
    batch.Clear();

    auto ops = trxColl->stealTrackedIndexOperations();
    if (!ops.empty()) {
      TRI_ASSERT(ridx.hasSelectivityEstimate() && ops.size() == 1);
      auto it = ops.begin();
      TRI_ASSERT(ridx.id() == it->first);
      
      if (foreground) {
        for (uint64_t hash : it->second.inserts) {
          ridx.estimator()->insert(hash);
        }
        for (uint64_t hash : it->second.removals) {
          ridx.estimator()->remove(hash);
        }
      } else {
        uint64_t seq = rootDB->GetLatestSequenceNumber();
        // since cuckoo estimator uses a map with seq as key we need to 
        ridx.estimator()->bufferUpdates(seq, std::move(it->second.inserts),
                                             std::move(it->second.removals));
      }
    }
  };

  OperationOptions options;
  for (it->Seek(bounds.start()); it->Valid(); it->Next()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    if (ridx.collection().vocbase().server().isStopping()) {
      res.reset(TRI_ERROR_SHUTTING_DOWN);
      break;
    }

    res = ridx.insert(trx, &batched, RocksDBKey::documentId(it->key()),
                      VPackSlice(reinterpret_cast<uint8_t const*>(it->value().data())),
                      options);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;

    if (numDocsWritten % 200 == 0) {  // commit buffered writes
      commitLambda();
      // cppcheck-suppress identicalConditionAfterEarlyExit
      if (res.fail()) {
        break;
      }
    }
  }

  if (!it->status().ok() && res.ok()) {
    res = rocksutils::convertStatus(it->status(), rocksutils::StatusHint::index);
  }

  if (res.ok()) {
    commitLambda();
  }

  if (res.ok()) {  // required so iresearch commits
    res = trx.commit();

    if (ridx.estimator() != nullptr) {
      ridx.estimator()->setAppliedSeq(rootDB->GetLatestSequenceNumber());
    }
  }

  // if an error occured drop() will be called
  LOG_TOPIC("dfa3b", DEBUG, Logger::ENGINES)
      << "SNAPSHOT CAPTURED " << numDocsWritten << " " << res.errorMessage();

  return res;
}

arangodb::Result RocksDBBuilderIndex::fillIndexForeground() {
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);

  const rocksdb::Snapshot* snap = nullptr;

  Result res;
  if (this->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods, true>(*internal, batch, snap);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatch, RocksDBBatchedMethods, true>(*internal, batch, snap);
  }

  return res;
}

namespace {

template <typename MethodsType>
struct ReplayHandler final : public rocksdb::WriteBatch::Handler {
  ReplayHandler(uint64_t oid, RocksDBIndex& idx, transaction::Methods& trx, MethodsType* methods)
      : _objectId(oid), _index(idx), _trx(trx), _methods(methods) {}

  bool Continue() override {
    if (_index.collection().vocbase().server().isStopping()) {
      tmpRes.reset(TRI_ERROR_SHUTTING_DOWN);
    }
    return tmpRes.ok();
  }

  uint64_t numInserted = 0;
  uint64_t numRemoved = 0;
  Result tmpRes;

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
    _lastObjectID = 0;
  }

  uint64_t endBatch() {
    _lastObjectID = 0;
    return _currentSequence;
  }

  // The default implementation of LogData does nothing.
  void LogData(const rocksdb::Slice& blob) override {
    switch (RocksDBLogValue::type(blob)) {
      case RocksDBLogType::TrackedDocumentInsert:
        if (_lastObjectID == _objectId) {
          auto pair = RocksDBLogValue::trackedDocument(blob);
          OperationOptions options;
          tmpRes = _index.insert(_trx, _methods, pair.first, pair.second, options);
          numInserted++;
        }
        break;

      case RocksDBLogType::TrackedDocumentRemove:
        if (_lastObjectID == _objectId) {
          auto pair = RocksDBLogValue::trackedDocument(blob);
          tmpRes = _index.remove(_trx, _methods, pair.first, pair.second,
                                 Index::OperationMode::normal);
          numRemoved++;
        }
        break;

      default:  // ignore
        _lastObjectID = 0;
        break;
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                        rocksdb::Slice const& value) override {
    incTick();
    if (column_family_id == RocksDBColumnFamily::definitions()->GetID()) {
      _lastObjectID = 0;
    } else if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      _lastObjectID = RocksDBKey::objectId(key);
    }

    return rocksdb::Status();
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
    incTick();
    if (column_family_id == RocksDBColumnFamily::definitions()->GetID()) {
      _lastObjectID = 0;
    } else if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      _lastObjectID = RocksDBKey::objectId(key);
    }
    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
    incTick();
    if (column_family_id == RocksDBColumnFamily::definitions()->GetID()) {
      _lastObjectID = 0;
    } else if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
      _lastObjectID = RocksDBKey::objectId(key);
    }
    return rocksdb::Status();
  }

  rocksdb::Status DeleteRangeCF(uint32_t column_family_id, const rocksdb::Slice& begin_key,
                                const rocksdb::Slice& end_key) override {
    incTick();  // drop and truncate may use this
    if (column_family_id == _index.columnFamily()->GetID() &&
        RocksDBKey::objectId(begin_key) == _objectId &&
        RocksDBKey::objectId(end_key) == _objectId) {
      _index.afterTruncate(_currentSequence, &_trx);
    }
    return rocksdb::Status();  // make WAL iterator happy
  }

 private:
  // tick function that is called before each new WAL entry
  void incTick() {
    if (_startOfBatch) {
      // we are at the start of a batch. do NOT increase sequence number
      _startOfBatch = false;
    } else {
      // we are inside a batch already. now increase sequence number
      ++_currentSequence;
    }
  }

 private:
  const uint64_t _objectId;  /// collection objectID
  RocksDBIndex& _index;      /// the index to use
  transaction::Methods& _trx;
  MethodsType* _methods;  /// methods to fill

  rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;
  rocksdb::SequenceNumber _lastWrittenSequence;
  bool _startOfBatch = false;
  uint64_t _lastObjectID = 0;
};

template <typename WriteBatchType, typename MethodsType>
Result catchup(RocksDBIndex& ridx, WriteBatchType& wb, AccessMode::Type mode,
               rocksdb::SequenceNumber startingFrom, rocksdb::SequenceNumber& lastScannedTick,
               uint64_t& numScanned, bool haveExclusiveAccess) {
  LogicalCollection& coll = ridx.collection();
  ::BuilderTrx trx(transaction::StandaloneContext::Create(coll.vocbase()), coll, mode);
  if (mode == AccessMode::Type::EXCLUSIVE) {
    trx.addHint(transaction::Hints::Hint::LOCK_NEVER);
  }
  Result res = trx.begin();
  if (res.fail()) {
    return res;
  }

  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBTransactionCollection* trxColl = trx.resolveTrxCollection();
  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());

  rocksdb::DB* rootDB = rocksutils::globalRocksDB()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);

  // write batch will be reset every x documents
  MethodsType batched(state, &wb);

  ReplayHandler<MethodsType> replay(rcoll->objectId(), ridx, trx, &batched);

  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  // no need verifying the WAL contents
  rocksdb::TransactionLogIterator::ReadOptions ro(false);
  rocksdb::Status s = rootDB->GetUpdatesSince(startingFrom, &iterator, ro);
  if (!s.ok()) {
    return res.reset(convertStatus(s, rocksutils::StatusHint::wal));
  }

  auto commitLambda = [&](rocksdb::SequenceNumber seq) {
    if (wb.GetWriteBatch()->Count() > 0) {
      rocksdb::WriteOptions wo;
      rocksdb::Status s = rootDB->Write(wo, wb.GetWriteBatch());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      }
    }
    wb.Clear();

    auto ops = trxColl->stealTrackedIndexOperations();
    if (!ops.empty()) {
      TRI_ASSERT(ridx.hasSelectivityEstimate() && ops.size() == 1);
      auto it = ops.begin();
      TRI_ASSERT(ridx.id() == it->first);
      ridx.estimator()->bufferUpdates(seq, std::move(it->second.inserts),
                                      std::move(it->second.removals));
    }
  };

  LOG_TOPIC("fa362", DEBUG, Logger::ENGINES) << "Scanning from " << startingFrom;

  for (; iterator->Valid(); iterator->Next()) {
    rocksdb::BatchResult batch = iterator->GetBatch();
    lastScannedTick = batch.sequence;  // start of the batch
    if (batch.sequence < startingFrom) {
      continue;  // skip
    }

    replay.startNewBatch(batch.sequence);
    s = batch.writeBatchPtr->Iterate(&replay);
    if (!s.ok()) {
      res = rocksutils::convertStatus(s);
      break;
    }
    if (replay.tmpRes.fail()) {
      res = replay.tmpRes;
      break;
    }

    commitLambda(batch.sequence);
    if (res.fail()) {
      break;
    }
    lastScannedTick = replay.endBatch();
  }

  s = iterator->status();
  // we can ignore it if we get a try again return value, because that either
  // indicates a write to another collection, or a write to this collection if
  // we are not in exclusive mode, in which case we will call catchup again
  if (!s.ok() && res.ok() && !s.IsTryAgain()) {
    LOG_TOPIC("8e3a4", WARN, Logger::ENGINES) << "iterator error '" <<
      s.ToString() << "'";
    res = rocksutils::convertStatus(s);
  }

  if (res.ok()) {
    numScanned = replay.numInserted + replay.numRemoved;
    res = trx.commit();  // important for iresearch
  }

  LOG_TOPIC("5796c", DEBUG, Logger::ENGINES) << "WAL REPLAYED insertions: " << replay.numInserted
                                    << "; deletions: " << replay.numRemoved
                                    << "; lastScannedTick " << lastScannedTick;

  return res;
}
}  // namespace

bool RocksDBBuilderIndex::Locker::lock() {
  if (!_locked) {
    if (_collection->lockWrite() != TRI_ERROR_NO_ERROR) {
      return false;
    }
    _locked = true;
  }
  return true;
}

void RocksDBBuilderIndex::Locker::unlock() {
  if (_locked) {
    _collection->unlockWrite();
    _locked = false;
  }
}

// Background index filler task
arangodb::Result RocksDBBuilderIndex::fillIndexBackground(Locker& locker) {
  TRI_ASSERT(locker.isLocked());

  arangodb::Result res;
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);

  RocksDBEngine* engine = globalRocksEngine();
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  rocksdb::Snapshot const* snap = rootDB->GetSnapshot();
  auto scope = scopeGuard([&] {
    if (snap) {
      rootDB->ReleaseSnapshot(snap);
    }
  });
  locker.unlock();
  
  // Step 1. Capture with snapshot
  if (internal->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods, false>(*internal, batch, snap);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatch, RocksDBBatchedMethods, false>(*internal, batch, snap);
  }
  
  if (res.fail()) {
    return res;
  }

  rocksdb::SequenceNumber scanFrom = snap->GetSequenceNumber();

  // Step 2. Scan the WAL for documents without lock
  int maxCatchups = 3;
  rocksdb::SequenceNumber lastScanned = 0;
  uint64_t numScanned = 0;
  do {
    lastScanned = 0;
    numScanned = 0;
    if (internal->unique()) {
      const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
      // unique index. we need to keep track of all our changes because we need
      // to avoid duplicate index keys. must therefore use a WriteBatchWithIndex
      rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
      res = ::catchup<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(
          *internal, batch, AccessMode::Type::WRITE, scanFrom, lastScanned, numScanned, false);
    } else {
      // non-unique index. all index keys will be unique anyway because they
      // contain the document id we can therefore get away with a cheap WriteBatch
      rocksdb::WriteBatch batch(32 * 1024 * 1024);
      res = ::catchup<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, batch,
                                                                  AccessMode::Type::WRITE,
                                                                  scanFrom, lastScanned,
                                                                  numScanned, false);
    }

    if (res.fail() && !res.is(TRI_ERROR_ARANGO_TRY_AGAIN)) {
      return res;
    }
    
    scanFrom = lastScanned;
  } while (maxCatchups-- > 0 && numScanned > 5000);

  if (!locker.lock()) {  // acquire exclusive collection lock
    return res.reset(TRI_ERROR_LOCK_TIMEOUT);
  }

  // Step 3. Scan the WAL for documents with a lock
  
  scanFrom = lastScanned;
  if (internal->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::catchup<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(
        *internal, batch, AccessMode::Type::EXCLUSIVE, scanFrom, lastScanned,
        numScanned, true);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::catchup<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, batch,
                                                                AccessMode::Type::EXCLUSIVE,
                                                                scanFrom, lastScanned,
                                                                numScanned, true);
  }

  return res;
}
