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

#include "Basics/HashSet.h"
#include "Basics/VelocyPackHelper.h"
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
    arangodb::HashSet<LocalDocumentId::BaseType> removed;
  };
}  // namespace

RocksDBBuilderIndex::RocksDBBuilderIndex(std::shared_ptr<arangodb::RocksDBIndex> const& wp)
    : RocksDBIndex(wp->id(), wp->collection(), wp->fields(), wp->unique(),
                   wp->sparse(), wp->columnFamily(), wp->objectId(),
                   /*useCache*/ false),
      _wrapped(wp),
      _trxCallback(nullptr),
      _hasError(false) {
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
                                   OperationMode mode) {
  return Result(); // do nothing
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

    if (!ctx) {
      return arangodb::Result(TRI_ERROR_INTERNAL,
                              std::string("failed to store state into a TransactionState for "
                                          "removal while building index'"));
    }
  }
  
  // do not track document more than once
  if (ctx->removed.find(documentId.id()) == ctx->removed.end()) {
    ctx->removed.insert(documentId.id());
    RocksDBLogValue val = RocksDBLogValue::TrackedDocumentRemove(slice);
    mthd->PutLogData(val.slice());
  }
  return Result();
}

// fast mode assuming exclusive access locked from outside
template <typename WriteBatchType, typename MethodsType>
static arangodb::Result fillIndex(RocksDBIndex& ridx,
                                  WriteBatchType& batch,
                                  std::function<void()> const& unlock,
                                  rocksdb::SequenceNumber& seq) {

  // fillindex can be non transactional, we just need to clean up
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);

  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(ridx.collection().getPhysical());
  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  rocksdb::Slice upper(bounds.end());

  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false;  // TODO set to true eventually

  const rocksdb::Snapshot* snap = rootDB->GetSnapshot();
  auto snapGuard = scopeGuard([&] { rootDB->ReleaseSnapshot(snap); });
  seq = snap->GetSequenceNumber();

  rocksdb::ReadOptions ro;
  ro.snapshot = snap;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;

  rocksdb::ColumnFamilyHandle* docCF = RocksDBColumnFamily::documents();
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));
  
  auto mode = AccessMode::Type::EXCLUSIVE;
  if (unlock) {
    mode = AccessMode::Type::WRITE;
    unlock(); // unlock after aquiring the snapshot
  }
  
  LogicalCollection& coll = ridx.collection();
  ::BuilderTrx trx(transaction::StandaloneContext::Create(coll.vocbase()), coll, mode);
  if (mode == AccessMode::Type::EXCLUSIVE) {
    trx.addHint(transaction::Hints::Hint::LOCK_NEVER);
  }
  Result res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  uint64_t numDocsWritten = 0;
  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBTransactionCollection* trxColl = trx.resolveTrxCollection();
  // write batch will be reset every x documents
  MethodsType batched(state, &batch);

  auto commitLambda = [&](rocksdb::SequenceNumber seq) {
    if (batch.GetWriteBatch()->Count() > 0) {
      s = rootDB->Write(wo, batch.GetWriteBatch());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
      }
    }
    batch.Clear();

    auto ops = trxColl->stealTrackedOperations();
    if (!ops.empty()) {
      TRI_ASSERT(ridx.hasSelectivityEstimate() && ops.size() == 1);
      auto it = ops.begin();
      TRI_ASSERT(ridx.id() == it->first);
      ridx.estimator()->bufferUpdates(seq, std::move(it->second.inserts),
                                      std::move(it->second.removals));
    }
  };

  it->Seek(bounds.start());
  while (it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);
    if (application_features::ApplicationServer::isStopping()) {
      res.reset(TRI_ERROR_SHUTTING_DOWN);
      break;
    }

    res = ridx.insert(trx, &batched, RocksDBKey::documentId(it->key()),
                      VPackSlice(it->value().data()), Index::OperationMode::normal);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;

    if (numDocsWritten % 200 == 0) {  // commit buffered writes
      commitLambda(rootDB->GetLatestSequenceNumber());
      if (res.fail()) {
        break;
      }
    }
    
    it->Next();
  }

  if (res.ok()) {
    commitLambda(rootDB->GetLatestSequenceNumber());
  }
  batch.Clear();
  
  if (res.ok()) {  // required so iresearch commits
    res = trx.commit();
  }

  // we will need to remove index elements created before an error
  // occurred, this needs to happen since we are non transactional
  if (res.fail()) {
    RocksDBKeyBounds bounds = ridx.getBounds();
    arangodb::Result res2 =
        rocksutils::removeLargeRange(rocksutils::globalRocksDB(), bounds, true,
                                     /*useRangeDel*/ numDocsWritten > 25000);
    if (res2.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "was not able to roll-back "
                                       << "index creation: " << res2.errorMessage();
    }
  }
  
  LOG_DEVEL << "SNAPSHOT CAPTURED " << numDocsWritten << " " << res.errorMessage();

  return res;
}

/// non-transactional: fill index with existing documents
/// from this collection
arangodb::Result RocksDBBuilderIndex::fillIndexFast() {
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);
  std::function<void()> empty;
  
  Result res;
  rocksdb::SequenceNumber snapSeq = 0;
  if (this->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(*internal, batch, empty, snapSeq);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, batch, empty, snapSeq);
  }
  
  return res;
}

namespace {
  template <typename MethodsType>
  struct ReplayHandler final : public rocksdb::WriteBatch::Handler {
    ReplayHandler(uint64_t oid,
                  RocksDBIndex& idx,
                  transaction::Methods& trx,
                  MethodsType* methods) : _objectId(oid), _index(idx),
    _trx(trx), _methods(methods) {}
    
    bool Continue() override {
      return tmpRes.ok();
    }
    
    uint64_t numDocs = 0;
    
    void startNewBatch(rocksdb::SequenceNumber startSequence) {
      _withinTransaction = false;
      _removedDocId.clear();
      
      // starting new write batch
      _startSequence = startSequence;
      _currentSequence = startSequence;
      _startOfBatch = true;
    }
    
    uint64_t endBatch() {
      _withinTransaction = false;
      _removedDocId.clear();
      return _currentSequence;
    }
    
    // The default implementation of LogData does nothing.
    void LogData(const rocksdb::Slice& blob) override {
      RocksDBLogType type = RocksDBLogValue::type(blob);
      switch (type) {
        case RocksDBLogType::BeginTransaction:
        case RocksDBLogType::SinglePut:
        case RocksDBLogType::SingleRemove: // deprecated
        case RocksDBLogType::SingleRemoveV2:
          _withinTransaction = true;
          break;
          
        case RocksDBLogType::TrackedDocumentRemove:
          if (_withinTransaction && _removedDocId) {
            VPackSlice slice = RocksDBLogValue::trackedDocument(blob);
            tmpRes = _index.remove(_trx, _methods, _removedDocId, slice, Index::OperationMode::normal);
            _removedDocId.clear();
          }
          break;
          
        case RocksDBLogType::CollectionTruncate: { // not allowed during index creation
          uint64_t objectId = RocksDBLogValue::objectId(blob);
          TRI_ASSERT(_objectId != objectId);
          FALLTHROUGH_INTENDED;
        }
          
        default: // ignore
          _withinTransaction = false;
          _removedDocId.clear();
          break;
      }
    }
    
    rocksdb::Status PutCF(uint32_t column_family_id, const rocksdb::Slice& key,
                          rocksdb::Slice const& value) override {
      incTick();
      _removedDocId.clear();
      if (column_family_id == RocksDBColumnFamily::definitions()->GetID()) {
        _withinTransaction = false;
      } else if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
        uint64_t oid = RocksDBKey::objectId(key);
        if (_withinTransaction && _objectId == oid) {
          LocalDocumentId docId = RocksDBKey::documentId(key);
          VPackSlice slice = RocksDBValue::data(value);
          tmpRes = _index.insert(_trx, _methods, docId, slice, Index::OperationMode::normal);
          numDocs++;
        }
      }
      
      return rocksdb::Status();
    }
    
    rocksdb::Status DeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
      incTick();
      if (column_family_id == RocksDBColumnFamily::definitions()->GetID()) {
        _withinTransaction = false;
      } else if (column_family_id == RocksDBColumnFamily::documents()->GetID()) {
        uint64_t oid = RocksDBKey::objectId(key);
        if (_withinTransaction && _objectId == oid) {
          _removedDocId = RocksDBKey::documentId(key);
        }
      }
      return rocksdb::Status();
    }
    
    rocksdb::Status SingleDeleteCF(uint32_t column_family_id, const rocksdb::Slice& key) override {
      return DeleteCF(column_family_id, key); // same handler
    }
    
    rocksdb::Status DeleteRangeCF(uint32_t /*column_family_id*/,
                                  const rocksdb::Slice& /*begin_key*/,
                                  const rocksdb::Slice& /*end_key*/) override {
      incTick(); // drop and truncate may use this
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
    const uint64_t _objectId; /// collection objectID
    RocksDBIndex& _index; /// the index to use
    transaction::Methods& _trx;
    MethodsType* _methods; /// methods to fill
    
    rocksdb::SequenceNumber _startSequence;
    rocksdb::SequenceNumber _currentSequence;
    rocksdb::SequenceNumber _lastWrittenSequence;
    bool _startOfBatch = false;
    
    Result tmpRes;
    bool _withinTransaction = false;
    LocalDocumentId _removedDocId;
  };
  
  template <typename WriteBatchType, typename MethodsType>
  Result catchup(RocksDBIndex& ridx, LogicalCollection& coll, WriteBatchType& wb,
                 AccessMode::Type mode,
                 rocksdb::SequenceNumber startingFrom,
                 rocksdb::SequenceNumber& lastScannedTick) {
    ::BuilderTrx trx(transaction::StandaloneContext::Create(coll.vocbase()), coll, mode);
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
        s = rootDB->Write(wo, wb.GetWriteBatch());
      }
      wb.Clear();
      
      auto ops = trxColl->stealTrackedOperations();
      if (!ops.empty()) {
        TRI_ASSERT(ridx.hasSelectivityEstimate() && ops.size() == 1);
        auto it = ops.begin();
        TRI_ASSERT(ridx.id() == it->first);
        ridx.estimator()->bufferUpdates(seq, std::move(it->second.inserts),
                                        std::move(it->second.removals));
      }
    };
    
    LOG_DEVEL << "Scanning from " << startingFrom;

    while (iterator->Valid() && s.ok()) {
      if (!(s = iterator->status()).ok()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "error during WAL scan: " << s.ToString();
        break;  // s is considered in the end
      }
      if (application_features::ApplicationServer::isStopping()) {
        res.reset(TRI_ERROR_SHUTTING_DOWN);
        break;
      }
      
      rocksdb::BatchResult batch = iterator->GetBatch();
      lastScannedTick = batch.sequence;  // start of the batch
      
      if (batch.sequence < startingFrom) {
        // LOG_DEVEL << "skipping batch from " << batch.sequence << " to "
        //<< (batch.sequence + batch.writeBatchPtr->Count());
        iterator->Next();  // skip
        continue;
      }
      
      replay.startNewBatch(batch.sequence);
      s = batch.writeBatchPtr->Iterate(&replay);
      if (!s.ok() || res.fail()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "error during WAL scan: " << s.ToString();
        break;  // s is considered in the end
      }
      commitLambda(batch.sequence);
      lastScannedTick = replay.endBatch();
      iterator->Next();
    }
    
    if (!s.ok() && res.ok()) {
      res = rocksutils::convertStatus(s);
    }
    
    if (res.ok()) {
      res = trx.commit(); // important for iresearch
    }
    
    if (res.fail()) {
      LOG_DEVEL << res.errorMessage();
    }
    
    LOG_DEVEL << "WAL REPLAYED " << replay.numDocs << " lastScannedTick " << lastScannedTick;
    
    return res;
  }
}

// Background index filler task
arangodb::Result RocksDBBuilderIndex::fillIndexBackground(std::function<void()> const& unlock) {
  arangodb::Result res;
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);
  
  RocksDBEngine* engine = globalRocksEngine();
  auto scope = scopeGuard([&] {
    engine->disableWalFilePruning(false);
  });
  engine->disableWalFilePruning(true);

  rocksdb::SequenceNumber snapSeq = 0;
  if (internal->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(*internal, batch, unlock, snapSeq);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::fillIndex<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, batch, unlock, snapSeq);
  }
  
  if (res.fail()) {
    return res;
  }
  TRI_ASSERT(snapSeq > 0);
  
  rocksdb::SequenceNumber lastScanned = 0;
  if (internal->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    res = ::catchup<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(*internal, _collection, batch, AccessMode::Type::WRITE, snapSeq, lastScanned);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    res = ::catchup<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, _collection, batch, AccessMode::Type::WRITE, snapSeq, lastScanned);
  }
  
  if (res.fail()) {
    return res;
  }
  
  if (snapSeq < lastScanned) {
    rocksdb::SequenceNumber last = 0;
    if (internal->unique()) {
      const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
      // unique index. we need to keep track of all our changes because we need to
      // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
      rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
      res = ::catchup<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(*internal, _collection, batch, AccessMode::Type::EXCLUSIVE, lastScanned, last);
    } else {
      // non-unique index. all index keys will be unique anyway because they
      // contain the document id we can therefore get away with a cheap WriteBatch
      rocksdb::WriteBatch batch(32 * 1024 * 1024);
      res = ::catchup<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, _collection, batch, AccessMode::Type::EXCLUSIVE, lastScanned, last);
    }
  }
  
  return res;
}
