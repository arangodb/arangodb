////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBTimeseries.h"

#include "Aql/PlanCache.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionCollection.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Helpers.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;


namespace {
  using namespace arangodb;
  
  class TimeIndexIterator final : public IndexIterator {
  public:
    TimeIndexIterator(LogicalCollection* collection,
                      transaction::Methods* trx)
    : IndexIterator(collection, trx) {}
    
    char const* typeName() const override { return "time-index-iterator"; }
    
    bool next(LocalDocumentIdCallback const& cb, size_t limit) override {
      return false;
    }
    
    void reset() override {  }
    
    void skip(uint64_t count, uint64_t& skipped) override {
      
    }
  };
  
  class RocksDBTimeIndex final : public RocksDBIndex {
  public:
    RocksDBTimeIndex() = delete;
    
    RocksDBTimeIndex(arangodb::LogicalCollection& collection,
                     std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes)
    : RocksDBIndex(0, collection, StaticStrings::IndexNameTime,
                   attributes, /*unique*/false, /*sparse*/false, RocksDBColumnFamily::time(),
                   /*objectId*/0, /*useCache*/false) {
      TRI_ASSERT(_cf == RocksDBColumnFamily::primary());
      TRI_ASSERT(_objectId != 0);
    }
    
    ~RocksDBTimeIndex() {}
    
    IndexType type() const override { return Index::TRI_IDX_TYPE_TIMESERIES; }
    
    char const* typeName() const override { return "timeseries"; }
    
    bool canBeDropped() const override { return false; }
    
    bool hasCoveringIterator() const override { return true; }
    
    bool isSorted() const override { return true; }
    
    bool hasSelectivityEstimate() const override { return true; }
    
    double selectivityEstimate(arangodb::velocypack::StringRef const& = arangodb::velocypack::StringRef()) const override {
      return 1.0;
    }
    
    void toVelocyPack(VPackBuilder& builder, std::underlying_type<Index::Serialize>::type flags) const override {
      builder.openObject();
      RocksDBIndex::toVelocyPack(builder, flags);
      builder.close();
    }
    
    Result insert(transaction::Methods& trx, RocksDBMethods* methods,
                  LocalDocumentId const& documentId,
                  arangodb::velocypack::Slice const& doc,
                  Index::OperationMode mode) override {
      return Result(TRI_ERROR_NOT_IMPLEMENTED);
    }
    
    /// remove index elements and put it in the specified write batch.
    Result remove(transaction::Methods& trx, RocksDBMethods* methods,
                  LocalDocumentId const& documentId,
                  arangodb::velocypack::Slice const& doc,
                  Index::OperationMode mode) override {
      return Result(TRI_ERROR_NOT_IMPLEMENTED);
    }
    
    Result update(transaction::Methods& trx, RocksDBMethods* methods,
                  LocalDocumentId const& oldDocumentId,
                  arangodb::velocypack::Slice const& oldDoc,
                  LocalDocumentId const& newDocumentId,
                  velocypack::Slice const& newDoc, Index::OperationMode mode) override {
      return Result(TRI_ERROR_NOT_IMPLEMENTED);
    }
    
    Index::UsageCosts supportsFilterCondition(std::vector<std::shared_ptr<arangodb::Index>> const& allIndexes,
                                              arangodb::aql::AstNode const* node,
                                              arangodb::aql::Variable const* reference,
                                              size_t itemsInIndex) const override {
      Index::UsageCosts cost;
      cost.supportsCondition = false;
      return cost;
    }
    
    Index::UsageCosts supportsSortCondition(arangodb::aql::SortCondition const* node,
                                            arangodb::aql::Variable const* reference,
                                            size_t itemsInIndex) const override {
      Index::UsageCosts cost;
      cost.supportsCondition = false;
      return cost;
    }
    
    std::unique_ptr<IndexIterator> iteratorForCondition(transaction::Methods* trx,
                                                        arangodb::aql::AstNode const* node,
                                                        arangodb::aql::Variable const* reference,
                                                        IndexIteratorOptions const& opts) override {
      return nullptr;
    }
    
    arangodb::aql::AstNode* specializeCondition(arangodb::aql::AstNode* node,
                                                arangodb::aql::Variable const* reference) const override {
      return node;
    }
  };
  
  
} // namespace

RocksDBTimeseries::RocksDBTimeseries(LogicalCollection& collection,
                                     VPackSlice const& info)
    : RocksDBMetaCollection(collection, info) {
  TRI_ASSERT(_logicalCollection.isAStub() || _objectId != 0);
}

RocksDBTimeseries::RocksDBTimeseries(LogicalCollection& collection,
                                     PhysicalCollection const* physical)
    : RocksDBMetaCollection(collection, VPackSlice::emptyObjectSlice()) {}

RocksDBTimeseries::~RocksDBTimeseries() {}

Result RocksDBTimeseries::updateProperties(VPackSlice const& slice, bool doSync) {
  // nothing else to do
  return TRI_ERROR_NO_ERROR;
}

PhysicalCollection* RocksDBTimeseries::clone(LogicalCollection& logical) const {
  return new RocksDBTimeseries(logical, this);
}

/// @brief export properties
void RocksDBTimeseries::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add("objectId", VPackValue(std::to_string(_objectId)));
  TRI_ASSERT(result.isOpenObject());
}

/// @brief closes an open collection
int RocksDBTimeseries::close() {
  unload();
  return TRI_ERROR_NO_ERROR;
}

void RocksDBTimeseries::load() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->load();
  }
}

void RocksDBTimeseries::unload() {
  READ_LOCKER(guard, _indexesLock);
  for (auto it : _indexes) {
    it->unload();
  }
}

/// return bounds for all documents
RocksDBKeyBounds RocksDBTimeseries::bounds() const {
  return RocksDBKeyBounds::CollectionTimeseries(_objectId);
}

void RocksDBTimeseries::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());

  StorageEngine* engine = EngineSelectorFeature::ENGINE;
  std::vector<std::shared_ptr<Index>> indexes;
  {
    READ_LOCKER(guard, _indexesLock);  // link creation needs read-lock too
    if (indexesSlice.length() == 0 && _indexes.empty()) {
      engine->indexFactory().fillSystemIndexes(_logicalCollection, indexes);
    } else {
      engine->indexFactory().prepareIndexes(_logicalCollection, indexesSlice, indexes);
    }
  }
  TRI_ASSERT(indexes.size() == 1);
  if (indexes.size() != 1 || indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_TIMESERIES) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

  WRITE_LOCKER(guard, _indexesLock);
  TRI_ASSERT(_indexes.empty());
  _indexes = std::move(indexes);
}

std::shared_ptr<Index> RocksDBTimeseries::createIndex(VPackSlice const& info,
                                                      bool restore, bool& created) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "index creation not allowed");
}

/// @brief Drop an index with the given iid.
bool RocksDBTimeseries::dropIndex(TRI_idx_iid_t iid) {
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED, "index dropping not allowed");
}

std::unique_ptr<IndexIterator> RocksDBTimeseries::getAllIterator(transaction::Methods* trx) const {
  return std::make_unique<RocksDBAllIndexIterator>(&_logicalCollection, trx);
}

std::unique_ptr<IndexIterator> RocksDBTimeseries::getAnyIterator(transaction::Methods* trx) const {
  return std::make_unique<RocksDBAnyIndexIterator>(&_logicalCollection, trx);
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

Result RocksDBTimeseries::truncate(transaction::Methods& trx, OperationOptions& options) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

LocalDocumentId RocksDBTimeseries::lookupKey(transaction::Methods* trx,
                                             VPackSlice const& key) const {
  TRI_ASSERT(key.isString());


}

bool RocksDBTimeseries::lookupRevision(transaction::Methods* trx, VPackSlice const& key,
                                       TRI_voc_rid_t& revisionId) const {
  TRI_ASSERT(key.isString());
  LocalDocumentId documentId;
  revisionId = 0;
  // lookup the revision id in the primary index
  if (!primaryIndex()->lookupRevision(trx, arangodb::velocypack::StringRef(key),
                                      documentId, revisionId)) {
    // document not found
    TRI_ASSERT(revisionId == 0);
    return false;
  }

  // document found, but revisionId may not have been present in the primary
  // index. this can happen for "older" collections
  TRI_ASSERT(documentId.isSet());

  // now look up the revision id in the actual document data

  return readDocumentWithCallback(trx, documentId, [&revisionId](LocalDocumentId const&, VPackSlice doc) {
    revisionId = transaction::helpers::extractRevFromDocument(doc);
  });
}

Result RocksDBCollection::read(transaction::Methods* trx,
                               arangodb::velocypack::StringRef const& key,
                               ManagedDocumentResult& result, bool /*lock*/) {
  Result res;
  do {
    LocalDocumentId const documentId = primaryIndex()->lookupKey(trx, key);
    if (!documentId.isSet()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      break;
    }  // else found
    
    std::string* buffer = result.setManaged();
    rocksdb::PinnableSlice ps(buffer);
    res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/true, /*fillCache*/true);
    if (res.ok()) {
      if (ps.IsPinned()) {
        buffer->assign(ps.data(), ps.size());
      } // else value is already assigned
      result.setRevisionId(); // extracts id from buffer
    }
  } while(res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
          RocksDBTransactionState::toState(trx)->setSnapshotOnReadOnly());
  return res;
}

// read using a token!
bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result) const {
  if (documentId.isSet()) {
    std::string* buffer = result.setManaged();
    rocksdb::PinnableSlice ps(buffer);
    Result res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/true, /*fillCache*/true);
    if (res.ok()) {
      if (ps.IsPinned()) {
        buffer->assign(ps.data(), ps.size());
      } // else value is already assigned
      return true;
    }
  }
  return false;
}

// read using a token!
bool RocksDBCollection::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& documentId,
                                                 IndexIterator::DocumentCallback const& cb) const {
  if (documentId.isSet()) {
    return lookupDocumentVPack(trx, documentId, cb, /*withCache*/true);
  }
  return false;
}

Result RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& resultMdr,
                                 OperationOptions& options,
                                 bool /*lock*/, KeyLockInfo* /*keyLockInfo*/,
                                 std::function<void()> const& cbDuringLock) {

  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  TRI_voc_tick_t revisionId;
  Result res(newObjectForInsert(trx, slice, isEdgeCollection, *builder.get(),
                                options.isRestore, revisionId));
  if (res.fail()) {
    return res;
  }

  VPackSlice newSlice = builder->slice();
  if (options.overwrite) {
    // special optimization for the overwrite case:
    // in case the operation is a RepSert, we will first check if the specified
    // primary key exists. we can abort this low-level insert early, before any
    // modification to the data has been done. this saves us from creating a
    // RocksDB transaction SavePoint. if we don't do the check here, we will
    // always create a SavePoint first and insert the new document. when then
    // inserting the key for the primary index and then detecting a unique
    // constraint violation, the transaction would be rolled back to the
    // SavePoint state, which will rebuild *all* data in the WriteBatch up to
    // the SavePoint. this can be super-expensive for bigger transactions. to
    // keep things simple, we are not checking for unique constraint violations
    // in secondary indexes here, but defer it to the regular index insertion
    // check
    VPackSlice keySlice = transaction::helpers::extractKeyFromDocument(newSlice);
    if (keySlice.isString()) {
      LocalDocumentId const oldDocumentId =
          primaryIndex()->lookupKey(trx, arangodb::velocypack::StringRef(keySlice));
      if (oldDocumentId.isSet()) {
        if (options.indexOperationMode == Index::OperationMode::internal) {
          // need to return the key of the conflict document
          return res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED,
                           keySlice.copyString());
        }
        return res.reset(TRI_ERROR_ARANGO_UNIQUE_CONSTRAINT_VIOLATED);
      }
    }
  }

  LocalDocumentId const documentId = LocalDocumentId::create();

  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  auto* state = RocksDBTransactionState::toState(trx);
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  res = insertDocument(trx, documentId, newSlice, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newSlice.begin());
      TRI_ASSERT(resultMdr.revisionId() == revisionId);
    } else if(!options.silent) {  //  need to pass revId manually
      transaction::BuilderLeaser keyBuilder(trx);
      keyBuilder->openObject(/*unindexed*/true);
      keyBuilder->add(StaticStrings::KeyString, transaction::helpers::extractKeyFromDocument(newSlice));
      keyBuilder->close();
      resultMdr.setManaged()->assign(reinterpret_cast<char const*>(keyBuilder->start()),
                                     keyBuilder->size());
      resultMdr.setRevisionId(revisionId);
    }

    bool hasPerformedIntermediateCommit = false;
    res = state->addOperation(_logicalCollection.id(), revisionId,
                              TRI_VOC_DOCUMENT_OPERATION_INSERT,
                              hasPerformedIntermediateCommit);

    if (res.ok() && cbDuringLock != nullptr) {
      cbDuringLock();
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::update(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const newSlice,
                                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                                 bool /*lock*/, ManagedDocumentResult& previousMdr) {

  VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);
  if (keySlice.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  } else if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const oldDocumentId = primaryIndex()->lookupKey(trx, VPackStringRef(keySlice));
  if (!oldDocumentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(trx, oldDocumentId, previousPS,
                                   /*readCache*/true, /*fillCache*/false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  if (!options.ignoreRevs) {  // Check old revision:
    TRI_voc_rid_t expectedRev = TRI_ExtractRevisionId(newSlice);
    int result = checkRevision(trx, expectedRev, previousMdr.revisionId());
    if (result != TRI_ERROR_NO_ERROR) {
      return res.reset(result);
    }
  }

  if (newSlice.length() <= 1) {  // TODO move above ?!
    // shortcut. no need to do anything
    resultMdr.setManaged(oldDoc.begin());
    TRI_ASSERT(!resultMdr.empty());

    trackWaitForSync(trx, options);
    return res;
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  LocalDocumentId const newDocumentId = LocalDocumentId::create();
  auto isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  res = mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                              options.mergeObjects, options.keepNull,
                              *builder.get(), options.isRestore, revisionId);
  if (res.fail()) {
    return res;
  }

  if (_isDBServer) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection, oldDoc, builder->slice(), true)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
    if (arangodb::smartJoinAttributeChanged(_logicalCollection, oldDoc, builder->slice(), true)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE);
    }
  }

  VPackSlice const newDoc(builder->slice());
  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_UPDATE);

  auto* state = RocksDBTransactionState::toState(trx);
  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_UPDATE);
  res = updateDocument(trx, oldDocumentId, oldDoc, newDocumentId, newDoc, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newDoc.begin());
      TRI_ASSERT(!resultMdr.empty());
    } else {  //  need to pass revId manually
      resultMdr.setRevisionId(revisionId);
    }
    if (options.returnOld) {
      if (previousPS.IsPinned()) { // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    auto result = state->addOperation(_logicalCollection.id(), revisionId,
                                      TRI_VOC_DOCUMENT_OPERATION_UPDATE,
                                      hasPerformedIntermediateCommit);
    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::replace(transaction::Methods* trx,
                                  arangodb::velocypack::Slice const newSlice,
                                  ManagedDocumentResult& resultMdr, OperationOptions& options,
                                  bool /*lock*/, ManagedDocumentResult& previousMdr) {

  VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);
  if (keySlice.isNone()) {
    return TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD;
  } else if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const oldDocumentId = primaryIndex()->lookupKey(trx, VPackStringRef(keySlice));
  if (!oldDocumentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(trx, oldDocumentId, previousPS,
                                   /*readCache*/true, /*fillCache*/false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  if (!options.ignoreRevs) {  // Check old revision:
    TRI_voc_rid_t expectedRev = TRI_ExtractRevisionId(newSlice);
    res = checkRevision(trx, expectedRev, previousMdr.revisionId());
    if (res.fail()) {
      return res;
    }
  }

  // merge old and new values
  TRI_voc_rid_t revisionId;
  LocalDocumentId const newDocumentId = LocalDocumentId::create();
  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  res = newObjectForReplace(trx, oldDoc, newSlice, isEdgeCollection,
                            *builder.get(), options.isRestore, revisionId);
  if (res.fail()) {
    return res;
  }

  if (_isDBServer) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection, oldDoc, builder->slice(), false)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
    if (arangodb::smartJoinAttributeChanged(_logicalCollection, oldDoc, builder->slice(), false)) {
      return Result(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE);
    }
  }

  VPackSlice const newDoc(builder->slice());
  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_REPLACE);

  auto* state = RocksDBTransactionState::toState(trx);
  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_REPLACE);
  res = updateDocument(trx, oldDocumentId, oldDoc, newDocumentId, newDoc, options);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newDoc.begin());
      TRI_ASSERT(!resultMdr.empty());
    } else {  //  need to pass revId manually
      resultMdr.setRevisionId(revisionId);
    }
    if (options.returnOld) {
      if (previousPS.IsPinned()) { // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    auto result = state->addOperation(_logicalCollection.id(), revisionId,
                                      TRI_VOC_DOCUMENT_OPERATION_REPLACE,
                                      hasPerformedIntermediateCommit);

    if (result.fail()) {
      THROW_ARANGO_EXCEPTION(result);
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

Result RocksDBCollection::remove(transaction::Methods& trx, velocypack::Slice slice,
                                 ManagedDocumentResult& previousMdr, OperationOptions& options,
                                 bool /*lock*/, KeyLockInfo* /*keyLockInfo*/,
                                 std::function<void()> const& cbDuringLock) {

  VPackSlice keySlice;
  if (slice.isString()) {
    keySlice = slice;
  } else {
    keySlice = slice.get(StaticStrings::KeyString);
  }
  TRI_ASSERT(!keySlice.isNone());
  if (!keySlice.isString()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  auto const documentId = primaryIndex()->lookupKey(&trx, VPackStringRef(keySlice));
  if (!documentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  Result res = lookupDocumentVPack(&trx, documentId, previousPS,
                                   /*readCache*/true, /*fillCache*/false);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId() != 0);

  // Check old revision:
  if (!options.ignoreRevs && slice.isObject()) {
    TRI_voc_rid_t expectedRevisionId = TRI_ExtractRevisionId(slice);
    res = checkRevision(&trx, expectedRevisionId, previousMdr.revisionId());

    if (res.fail()) {
      return res;
    }
  }

  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBSavePoint guard(&trx, TRI_VOC_DOCUMENT_OPERATION_REMOVE);

  // add possible log statement under guard
  state->prepareOperation(_logicalCollection.id(), previousMdr.revisionId(),
                          TRI_VOC_DOCUMENT_OPERATION_REMOVE);
  res = removeDocument(&trx, documentId, oldDoc, options);

  if (res.ok()) {
    trackWaitForSync(&trx, options);

    if (options.returnOld) {
      if (previousPS.IsPinned()) {  // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    bool hasPerformedIntermediateCommit = false;
    res = state->addOperation(_logicalCollection.id(), newRevisionId(), TRI_VOC_DOCUMENT_OPERATION_REMOVE,
                              hasPerformedIntermediateCommit);

    if (res.ok() && cbDuringLock != nullptr) {
      cbDuringLock();
    }

    guard.finish(hasPerformedIntermediateCommit);
  }

  return res;
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());

  uint64_t out = 0;
  db->GetApproximateSizes(RocksDBColumnFamily::documents(), &r, 1, &out,
                          static_cast<uint8_t>(
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));

  builder->add("documentsSize", VPackValue(out));
  bool cacheInUse = useCache();
  builder->add("cacheInUse", VPackValue(cacheInUse));
  if (cacheInUse) {
    builder->add("cacheSize", VPackValue(_cache->size()));
    builder->add("cacheUsage", VPackValue(_cache->usage()));
    auto hitRates = _cache->hitRates();
    double rate = hitRates.first;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder->add("cacheLifeTimeHitRate", VPackValue(rate));
    rate = hitRates.second;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder->add("cacheWindowedHitRate", VPackValue(rate));
  } else {
    builder->add("cacheSize", VPackValue(0));
    builder->add("cacheUsage", VPackValue(0));
  }
}

Result RocksDBCollection::insertDocument(arangodb::transaction::Methods* trx,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(_objectId, documentId);

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // blacklist new document to avoid caching without committing first
    blackListKey(key.ref());
  }
    
  RocksDBMethods* mthds = state->rocksdbMethods();
  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  TRI_ASSERT(key->containsLocalDocumentId(documentId));
  rocksdb::Status s =
      mthds->PutUntracked(RocksDBColumnFamily::documents(), key.ref(),
                          rocksdb::Slice(doc.startAs<char>(),
                                         static_cast<size_t>(doc.byteSize())));
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& idx : _indexes) {
    RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(idx.get());
    res = rIdx->insert(*trx, mthds, documentId, doc, options.indexOperationMode);

    if (res.fail()) {
      break;
    }
  }

  return res;
}

Result RocksDBCollection::removeDocument(arangodb::transaction::Methods* trx,
                                         LocalDocumentId const& documentId,
                                         VPackSlice const& doc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(_objectId != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(_objectId, documentId);

  blackListKey(key.ref());

  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);

  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  rocksdb::Status s = mthds->SingleDelete(RocksDBColumnFamily::documents(), key.ref());
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  /*LOG_TOPIC("17502", ERR, Logger::ENGINES)
      << "Delete rev: " << revisionId << " trx: " << trx->state()->id()
      << " seq: " << mthds->sequenceNumber()
      << " objectID " << _objectId << " name: " << _logicalCollection.name();*/

  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& idx : _indexes) {
    RocksDBIndex* ridx = static_cast<RocksDBIndex*>(idx.get());
    res = ridx->remove(*trx, mthds, documentId, doc, options.indexOperationMode);

    if (res.fail()) {
      break;
    }
  }

  return res;
}

Result RocksDBCollection::updateDocument(transaction::Methods* trx,
                                         LocalDocumentId const& oldDocumentId,
                                         VPackSlice const& oldDoc,
                                         LocalDocumentId const& newDocumentId,
                                         VPackSlice const& newDoc,
                                         OperationOptions& options) const {
  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(_objectId != 0);
  Result res;

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  RocksDBMethods* mthds = state->rocksdbMethods();
  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  RocksDBKeyLeaser key(trx);
  key->constructDocument(_objectId, oldDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(oldDocumentId));
  blackListKey(key.ref());

  rocksdb::Status s = mthds->SingleDelete(RocksDBColumnFamily::documents(), key.ref());
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  key->constructDocument(_objectId, newDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(newDocumentId));
  s = mthds->PutUntracked(RocksDBColumnFamily::documents(), key.ref(),
                          rocksdb::Slice(newDoc.startAs<char>(),
                                         static_cast<size_t>(newDoc.byteSize())));
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }
  
  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // blacklist new document to avoid caching without committing first
    blackListKey(key.ref());
  }
    
  READ_LOCKER(guard, _indexesLock);
  for (std::shared_ptr<Index> const& idx : _indexes) {
    RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(idx.get());
    res = rIdx->update(*trx, mthds, oldDocumentId, oldDoc, newDocumentId,
                       newDoc, options.indexOperationMode);

    if (res.fail()) {
      break;
    }
  }

  return res;
}

/// @brief lookup document in cache and / or rocksdb
/// @param readCache attempt to read from cache
/// @param fillCache fill cache with found document
arangodb::Result RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                                        LocalDocumentId const& documentId,
                                                        rocksdb::PinnableSlice& ps,
                                                        bool readCache,
                                                        bool fillCache) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(_objectId != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(_objectId, documentId);

  bool lockTimeout = false;
  if (readCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {  // copy finding into buffer
      ps.PinSelf(rocksdb::Slice(reinterpret_cast<char const*>(f.value()->value()),
                                f.value()->valueSize()));
      // TODO we could potentially use the PinSlice method ?!
      return res; // all good
    }
    if (f.result().errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      // assuming someone is currently holding a write lock, which
      // is why we cannot access the TransactionalBucket.
      lockTimeout = true;  // we skip the insert in this case
    }
  }

  RocksDBMethods* mthd = RocksDBTransactionState::toMethods(trx);
  rocksdb::Status s = mthd->Get(RocksDBColumnFamily::documents(), key->string(), &ps);

  if (!s.ok()) {
    LOG_TOPIC("f63dd", DEBUG, Logger::ENGINES)
    << "NOT FOUND rev: " << documentId.id() << " trx: " << trx->state()->id()
    << " objectID " << _objectId << " name: " << _logicalCollection.name();
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  if (fillCache && useCache() && !lockTimeout) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    auto entry =
        cache::CachedValue::construct(key->string().data(),
                                      static_cast<uint32_t>(key->string().size()),
                                      ps.data(), static_cast<uint64_t>(ps.size()));
    if (entry) {
      auto status = _cache->insert(entry);
      if (status.errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
        // the writeLock uses cpu_relax internally, so we can try yield
        std::this_thread::yield();
        status = _cache->insert(entry);
      }
      if (status.fail()) {
        delete entry;
      }
    }
  }

  return res;
}

bool RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                            LocalDocumentId const& documentId,
                                            IndexIterator::DocumentCallback const& cb,
                                            bool withCache) const {

  if (withCache && useCache()) {
    RocksDBKeyLeaser key(trx);
    key->constructDocument(_objectId, documentId);
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {
      cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(f.value()->value())));
      return true;
    }
  }

  transaction::StringLeaser buffer(trx);
  rocksdb::PinnableSlice ps(buffer.get());
  Result res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/false, withCache);
  if (res.ok()) {
    TRI_ASSERT(ps.size() > 0);
    cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));
    return true;
  }
  return false;
}
