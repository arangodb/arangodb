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
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
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

#include <velocypack/Iterator.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

RocksDBBuilderIndex::RocksDBBuilderIndex(std::shared_ptr<arangodb::RocksDBIndex> const& wp)
    : RocksDBIndex(wp->id(), wp->collection(), wp->fields(), wp->unique(),
                   wp->sparse(), wp->columnFamily(), wp->objectId(),
                   /*useCache*/ false),
      _wrapped(wp),
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
Result RocksDBBuilderIndex::insertInternal(transaction::Methods& trx, RocksDBMethods* mthd,
                                           LocalDocumentId const& documentId,
                                           arangodb::velocypack::Slice const& slice,
                                           OperationMode mode) {
  TRI_ASSERT(false);  // not enabled
  Result r = _wrapped->insertInternal(trx, mthd, documentId, slice, mode);
  if (r.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
    // these are expected errors; store in builder and suppress
    bool expected = false;
    if (!r.ok() && _hasError.compare_exchange_strong(expected, true)) {
      std::lock_guard<std::mutex> guard(_errorMutex);
      _errorResult = r;
    }
    return Result();
  }
  return r;
}

/// remove index elements and put it in the specified write batch.
Result RocksDBBuilderIndex::removeInternal(transaction::Methods& trx, RocksDBMethods* mthd,
                                           LocalDocumentId const& documentId,
                                           arangodb::velocypack::Slice const& slice,
                                           OperationMode mode) {
  TRI_ASSERT(false);  // not enabled
  {
    std::lock_guard<std::mutex> guard(_removedDocsMutex);
    _removedDocs.insert(documentId.id());
  }
  {  // wait for keys do be inserted, so we can remove them again
    std::unique_lock<std::mutex> guard(_lockedDocsMutex);
    if (_lockedDocs.find(documentId.id()) != _lockedDocs.end()) {
      _lockedDocsCond.wait(guard);
    }
  }

  Result r = _wrapped->removeInternal(trx, mthd, documentId, slice, mode);
  if (r.is(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED)) {
    // these are expected errors; store in builder and suppress
    bool expected = false;
    if (!r.ok() && _hasError.compare_exchange_strong(expected, true)) {
      std::lock_guard<std::mutex> guard(_errorMutex);
      _errorResult = r;
    }
    return Result();
  }
  return r;
}

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
}  // namespace

// Background index filler task
// FIXME simon: not used right now because rollbacks are not correctly handled
// yet
arangodb::Result RocksDBBuilderIndex::fillIndexBackground(std::function<void()> const& unlock) {
  arangodb::Result res;

  //  1. Index everything under a snapshot iterator (get snapshot under
  //  exclusive coll lock)
  //  2. Track deleted document IDs so we can avoid indexing them
  //  3. Avoid conflicts on unique index keys by using rocksdb::Transaction
  //  snapshot conflict checking
  //  4. Supress unique constraint violations / conflicts or client drivers

  auto lockedDocsGuard = scopeGuard([&] {  // clear all the processed documents
    std::lock_guard<std::mutex> guard(_lockedDocsMutex);
    _lockedDocs.clear();
    _lockedDocsCond.notify_all();
  });

  // fillindex can be non transactional, we just need to clean up
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(_collection.getPhysical());
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);

  uint64_t numDocsWritten = 0;

  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());

  rocksdb::Slice upper(bounds.end());  // exclusive upper bound
  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false;  // TODO set to true eventually

  // create a read-snapshot under the guard
  rocksdb::Snapshot const* snap = rootDB->GetSnapshot();
  auto snapGuard = scopeGuard([&] { rootDB->ReleaseSnapshot(snap); });
  TRI_ASSERT(snap != nullptr);

  rocksdb::ReadOptions ro;
  ro.snapshot = snap;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;

  rocksdb::ColumnFamilyHandle* docCF = bounds.columnFamily();
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));

  unlock();  // release indexes write lock
  // FIXME use buildertrx
  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(
                                      _collection.vocbase()),
                                  _collection, AccessMode::Type::WRITE);
  res = trx.begin();
  if (res.fail()) {
    return res;
  }
  auto state = RocksDBTransactionState::toState(&trx);

  // transaction used to perform actual indexing
  rocksdb::TransactionOptions to;
  to.lock_timeout = 100;  // 100ms
  std::unique_ptr<rocksdb::Transaction> rtrx(engine->db()->BeginTransaction(wo, to));
  if (this->unique()) {
    rtrx->SetSnapshot();  // needed for unique index conflict detection
  } else {
    rtrx->DisableIndexing();  // we never check for existing index keys
  }
  RocksDBSideTrxMethods batched(state, rtrx.get());

  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);

  // FIXE make selectivity estimates batch wise
  it->Seek(bounds.start());
  while (it->Valid() && it->key().compare(upper) < 0) {
    if (_hasError.load(std::memory_order_acquire)) {
      std::lock_guard<std::mutex> guard(_errorMutex);
      res = _errorResult;  // a Writer got an error
      break;
    }

    LocalDocumentId docId = RocksDBKey::documentId(it->key());
    {
      // must acquire both locks here to prevent interleaved operations
      std::lock_guard<std::mutex> guard(_removedDocsMutex);
      std::lock_guard<std::mutex> guard2(_lockedDocsMutex);
      if (_removedDocs.find(docId.id()) != _removedDocs.end()) {
        _removedDocs.erase(_removedDocs.find(docId.id()));
        it->Next();
        continue;
      }
      _lockedDocs.insert(docId.id());
    }

    res = internal->insertInternal(trx, &batched, docId, VPackSlice(it->value().data()),
                                   Index::OperationMode::normal);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;

    if (numDocsWritten % 200 == 0) {  // commit buffered writes
      s = rtrx->Commit();
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
        break;
      }
      {  // clear all the processed documents
        std::lock_guard<std::mutex> guard(_lockedDocsMutex);
        _lockedDocs.clear();
        _lockedDocsCond.notify_all();
      }
      engine->db()->BeginTransaction(wo, to, rtrx.get());  // reuse transaction
      if (this->unique()) {
        rtrx->SetSnapshot();
      }
    }

    it->Next();
  }

  // now actually write all remaining index keys
  if (res.ok() && rtrx->GetNumPuts() > 0) {
    s = rtrx->Commit();
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
    }
  }

  if (res.ok()) {
    res = trx.commit();  // required to commit selectivity estimates
  }

  return res;
}

// fast mode assuming exclusive access locked from outside
template <typename WriteBatchType, typename MethodsType>
static arangodb::Result fillIndexFast(RocksDBIndex& ridx, LogicalCollection& coll,
                                      WriteBatchType& batch) {
  Result res;
  ::BuilderTrx trx(transaction::StandaloneContext::Create(coll.vocbase()), coll,
                   AccessMode::Type::EXCLUSIVE);
  trx.addHint(transaction::Hints::Hint::LOCK_NEVER);  // already locked
  res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(coll.getPhysical());
  auto state = RocksDBTransactionState::toState(&trx);
  auto methds = RocksDBTransactionState::toMethods(&trx);
  RocksDBTransactionCollection* trxColl = trx.resolveTrxCollection();

  // fillindex can be non transactional, we just need to clean up
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);

  uint64_t numDocsWritten = 0;
  // write batch will be reset every x documents
  MethodsType batched(state, &batch);

  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  rocksdb::Slice upper(bounds.end());

  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false;  // TODO set to true eventually

  const rocksdb::Snapshot* snap = rootDB->GetSnapshot();
  auto snapGuard = scopeGuard([&] { rootDB->ReleaseSnapshot(snap); });

  rocksdb::ReadOptions ro;
  ro.snapshot = snap;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;

  rocksdb::ColumnFamilyHandle* docCF = RocksDBColumnFamily::documents();
  std::unique_ptr<rocksdb::Iterator> it = methds->NewIterator(ro, docCF);

  auto commitLambda = [&] {
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
      ridx.estimator()->bufferUpdates(it->first, std::move(it->second.inserts),
                                      std::move(it->second.removals));
    }
  };

  it->Seek(bounds.start());
  while (it->Valid()) {
    TRI_ASSERT(it->key().compare(upper) < 0);

    res = ridx.insertInternal(trx, &batched, RocksDBKey::documentId(it->key()),
                              VPackSlice(it->value().data()), Index::OperationMode::normal);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;

    if (numDocsWritten % 200 == 0) {  // commit buffered writes
      commitLambda();
      if (res.fail()) {
        break;
      }
    }

    it->Next();
  }

  if (res.ok()) {
    commitLambda();
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

  return res;
}

/// non-transactional: fill index with existing documents
/// from this collection
arangodb::Result RocksDBBuilderIndex::fillIndexFast() {
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);
  if (this->unique()) {
    const rocksdb::Comparator* cmp = internal->columnFamily()->GetComparator();
    // unique index. we need to keep track of all our changes because we need to
    // avoid duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(cmp, 32 * 1024 * 1024);
    return ::fillIndexFast<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(
        *internal, _collection, batch);
  } else {
    // non-unique index. all index keys will be unique anyway because they
    // contain the document id we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    return ::fillIndexFast<rocksdb::WriteBatch, RocksDBBatchedMethods>(*internal, _collection,
                                                                       batch);
  }
}
