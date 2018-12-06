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
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
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
  : RocksDBIndex(wp->id(), *wp->collection(),
                 wp->fields(), wp->unique(),
                 wp->sparse(), wp->columnFamily(), 0, false),
                 _wrapped(wp), _hasError(false) {
                   TRI_ASSERT(_wrapped);
}

/// @brief return a VelocyPack representation of the index
void RocksDBBuilderIndex::toVelocyPack(VPackBuilder& builder,
                                std::underlying_type<Serialize>::type flags) const {
  VPackBuilder inner;
  _wrapped->toVelocyPack(inner, flags);
  TRI_ASSERT(inner.slice().isObject());
  builder.openObject(); // FIXME refactor RocksDBIndex::toVelocyPack !!
  builder.add(velocypack::ObjectIterator(inner.slice()));
  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    builder.add(StaticStrings::IndexIsBuilding, VPackValue(true));
  }
  builder.close();
}

/// insert index elements into the specified write batch.
Result RocksDBBuilderIndex::insertInternal(transaction::Methods* trx, RocksDBMethods* mthd,
                                           LocalDocumentId const& documentId,
                                           arangodb::velocypack::Slice const& slice,
                                           OperationMode mode) {
  Result r = _wrapped->insertInternal(trx, mthd, documentId, slice, mode);
  if (!r.ok() && !_hasError.load(std::memory_order_acquire)) {
    _errorResult = r;
    _hasError.store(true, std::memory_order_release);
  }
  return Result();
}

//Result RocksDBBuilderIndex::updateInternal(transaction::Methods* trx, RocksDBMethods* mthd,
//                                           LocalDocumentId const& oldDocumentId,
//                                           arangodb::velocypack::Slice const& oldDoc,
//                                           LocalDocumentId const& newDocumentId,
//                                           arangodb::velocypack::Slice const& newDoc,
//                                           OperationMode mode) {
//  // It is illegal to call this method on the primary index
//  // RocksDBPrimaryIndex must override this method accordingly
//  TRI_ASSERT(type() != TRI_IDX_TYPE_PRIMARY_INDEX);
//
//  Result res = removeInternal(trx, mthd, oldDocumentId, oldDoc, mode);
//  if (!res.ok()) {
//    return res;
//  }
//  return insertInternal(trx, mthd, newDocumentId, newDoc, mode);
//}

/// remove index elements and put it in the specified write batch.
Result RocksDBBuilderIndex::removeInternal(transaction::Methods* trx, RocksDBMethods* mthd,
                                           LocalDocumentId const& documentId,
                                           arangodb::velocypack::Slice const& slice,
                                           OperationMode mode) {
  {
    std::lock_guard<std::mutex> guard(_removedDocsMutex);
    _removedDocs.insert(documentId.id());
  }
  { // wait for keys do be inserted, so we can remove them again
    std::unique_lock<std::mutex> guard(_lockedDocsMutex);
    if (_lockedDocs.find(documentId.id()) != _lockedDocs.end()) {
      _lockedDocsCond.wait(guard);
    }
  }
  
  Result r = _wrapped->removeInternal(trx, mthd, documentId, slice, mode);
  if (!r.ok() && !_hasError.load(std::memory_order_acquire)) {
    _errorResult = r;
    _hasError.store(true, std::memory_order_release);
  }
  return Result();
}

// Background index filler task
arangodb::Result RocksDBBuilderIndex::fillIndexBackground(std::function<void()> const& unlock) {
  arangodb::Result res;
  
  // fillindex can be non transactional, we just need to clean up
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  RocksDBCollection* rcoll = static_cast<RocksDBCollection*>(_collection.getPhysical());
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);
  
  uint64_t numDocsWritten = 0;
  
  auto bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
  rocksdb::Slice upper(bounds.end()); // exclusive upper bound
  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false; // TODO set to true eventually
  
  // create a read-snapshot under the guard
  rocksdb::Snapshot const* snap = rootDB->GetSnapshot();
  auto snapGuard = scopeGuard([&] {
    rootDB->ReleaseSnapshot(snap);
  });
  TRI_ASSERT(snap != nullptr);
  
  rocksdb::ReadOptions ro;
  ro.snapshot = snap;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;
  
  rocksdb::ColumnFamilyHandle* docCF = bounds.columnFamily();
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));
  
  unlock(); // release indexes write lock
  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(_collection.vocbase()),
                                  _collection, AccessMode::Type::WRITE);
  res = trx.begin();
  if (res.fail()) {
    return res;
  }
  auto state = RocksDBTransactionState::toState(&trx);
  
  // transaction used to perform actual indexing
  rocksdb::TransactionOptions to;
  to.lock_timeout = 100; // 100ms
  std::unique_ptr<rocksdb::Transaction> rtrx(engine->db()->BeginTransaction(wo, to));
  if (this->unique()) {
    rtrx->SetSnapshot(); // needed for unique index conflict detection
  } else { // FIXME use PutUntracked
    rtrx->DisableIndexing(); // we never check for existing index keys
  }
  RocksDBSubTrxMethods batched(state, rtrx.get());
  
  RocksDBIndex* internal = _wrapped.get();
  TRI_ASSERT(internal != nullptr);
  
  it->Seek(bounds.start());
  while (it->Valid() && it->key().compare(upper) < 0) {
    
    if (_hasError.load(std::memory_order_acquire)) {
      res = _errorResult; // a Writer got an error
      break;
    }
    
    LocalDocumentId docId = RocksDBKey::documentId(it->key());
    {
      std::lock_guard<std::mutex> guard(_removedDocsMutex);
      if (_removedDocs.find(docId.id()) != _removedDocs.end()) {
        continue;
      }
      std::lock_guard<std::mutex> guard2(_lockedDocsMutex);
      _lockedDocs.insert(docId.id());// must be done under _removedDocsMutex
    }
    
    res = internal->insertInternal(&trx, &batched, docId,
                                   VPackSlice(it->value().data()),
                                   Index::OperationMode::normal);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;
    
    if (numDocsWritten % 200 == 0) { // commit buffered writes
      s = rtrx->Commit();
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
        break;
      }
      { // clear all the processed documents
        std::lock_guard<std::mutex> guard2(_lockedDocsMutex);
        _lockedDocs.clear();
        _lockedDocsCond.notify_all();
      }
      engine->db()->BeginTransaction(wo, to, rtrx.get()); // reuse transaction
      if (this->unique()) {
        rtrx->SetSnapshot();
      }
    }
    
    it->Next();
  }
//
//  if (res.ok() && !toRevisit.empty()) { // now roll-up skipped keys
//    to.lock_timeout = 5000; // longer timeout to increase the odds
//    engine->db()->BeginTransaction(wo, to, rtrx.get()); // release keys
//    rtrx->SetSnapshot();
//    RocksDBKey key;
//
//    for (LocalDocumentId const& doc : toRevisit) {
//      key.constructDocument(coll->objectId(), doc);
//
//      rocksdb::PinnableSlice slice;
//      s = rtrx->GetForUpdate(ro, docCF, key.string(), &slice, /*exclusive*/false);
//      if (!s.ok()) {
//        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
//        break;
//      }
//      res = ridx->insertInternal(&trx, &batched, doc,
//                                 VPackSlice(slice.data()),
//                                 Index::OperationMode::normal);
//      if (res.fail()) {
//        break;
//      }
//
//      numDocsWritten++;
//    }
//  }
  
  // now actually write all remaining index keys
  if (res.ok() && rtrx->GetNumPuts() > 0) {
    s = rtrx->Commit();
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
    }
  }
  
  res = trx.commit(); // required to commit selectivity estimates
  
  // clear all the processed documents
  std::lock_guard<std::mutex> guard2(_lockedDocsMutex);
  _lockedDocs.clear();
  _lockedDocsCond.notify_all();
  
  // we will need to remove index elements created before an error
  // occurred, this needs to happen since we are non transactional
//  if (res.fail()) {
//    RocksDBKeyBounds bounds = internal->getBounds();
//    arangodb::Result res2 = rocksutils::removeLargeRange(rocksutils::globalRocksDB(), bounds,
//                                                         true, /*useRangeDel*/numDocsWritten > 25000);
//    if (res2.fail()) {
//      LOG_TOPIC(WARN, Logger::ENGINES) << "was not able to roll-back "
//      << "index creation: " << res2.errorMessage();
//    }
//  }
  
  return res;
}

// fast mode assuming exclusive access
template<typename WriteBatchType, typename MethodsType>
static arangodb::Result fillIndexFast(transaction::Methods& trx,
                                      RocksDBIndex* ridx,
                                      RocksDBCollection* coll,
                                      WriteBatchType& batch) {
  auto state = RocksDBTransactionState::toState(&trx);
  arangodb::Result res;
  
  // fillindex can be non transactional, we just need to clean up
  RocksDBEngine* engine = rocksutils::globalRocksEngine();
  rocksdb::DB* rootDB = engine->db()->GetRootDB();
  TRI_ASSERT(rootDB != nullptr);
  
  uint64_t numDocsWritten = 0;
  // write batch will be reset every x documents
  MethodsType batched(state, &batch);
  
  auto bounds = RocksDBKeyBounds::CollectionDocuments(coll->objectId());
  rocksdb::Slice upper(bounds.end());
  
  rocksdb::Status s;
  rocksdb::WriteOptions wo;
  wo.disableWAL = false; // TODO set to true eventually
  
  // we iterator without a snapshot
  rocksdb::ReadOptions ro;
  ro.prefix_same_as_start = true;
  ro.iterate_upper_bound = &upper;
  ro.verify_checksums = false;
  ro.fill_cache = false;
  
  rocksdb::ColumnFamilyHandle* docCF = bounds.columnFamily();
  std::unique_ptr<rocksdb::Iterator> it(rootDB->NewIterator(ro, docCF));
  
  it->Seek(bounds.start());
  while (it->Valid() && it->key().compare(upper) < 0) {
  
    res = ridx->insertInternal(&trx, &batched, RocksDBKey::documentId(it->key()),
                               VPackSlice(it->value().data()),
                               Index::OperationMode::normal);
    if (res.fail()) {
      break;
    }
    numDocsWritten++;
    
    if (numDocsWritten % 200 == 0) { // commit buffered writes
      s = rootDB->Write(wo, batch.GetWriteBatch());
      if (!s.ok()) {
        res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
        break;
      }
      batch.Clear();
    }
    
    it->Next();
  }
  
  if (res.ok() && batch.GetWriteBatch()->Count() > 0) { //
    s = rootDB->Write(wo, batch.GetWriteBatch());
    if (!s.ok()) {
      res = rocksutils::convertStatus(s, rocksutils::StatusHint::index);
    }
  }
  batch.Clear();
  
  // we will need to remove index elements created before an error
  // occurred, this needs to happen since we are non transactional
  if (res.fail()) {
    RocksDBKeyBounds bounds = ridx->getBounds();
    arangodb::Result res2 = rocksutils::removeLargeRange(rocksutils::globalRocksDB(), bounds,
                                                         true, /*useRangeDel*/numDocsWritten > 25000);
    if (res2.fail()) {
      LOG_TOPIC(WARN, Logger::ENGINES) << "was not able to roll-back "
      << "index creation: " << res2.errorMessage();
    }
  }
  
  return res;
}

/// non-transactional: fill index with existing documents
/// from this collection
arangodb::Result RocksDBBuilderIndex::fillIndex(std::function<void()> const& unlock) {
//  TRI_ASSERT(trx.state()->collection(_collection.id(), AccessMode::Type::WRITE));
  
#if 0
  RocksDBCollection* coll = static_cast<RocksDBCollection*>(_collection.getPhysical());
  unlock(); // we do not need the outer lock
  SingleCollectionTransaction trx(transaction::StandaloneContext::Create(_collection.vocbase()),
                                  _collection, AccessMode::Type::EXCLUSIVE);
  res = trx.begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  
  if (this->unique()) {
    // unique index. we need to keep track of all our changes because we need to avoid
    // duplicate index keys. must therefore use a WriteBatchWithIndex
    rocksdb::WriteBatchWithIndex batch(_cf->GetComparator(), 32 * 1024 * 1024);
    return ::fillIndexFast<rocksdb::WriteBatchWithIndex, RocksDBBatchedWithIndexMethods>(trx, this, coll, batch);
  } else {
    // non-unique index. all index keys will be unique anyway because they contain the document id
    // we can therefore get away with a cheap WriteBatch
    rocksdb::WriteBatch batch(32 * 1024 * 1024);
    return ::fillIndexFast<rocksdb::WriteBatch, RocksDBBatchedMethods>(trx, this, coll, batch);
  }
#endif
    return fillIndexBackground(unlock);
}
