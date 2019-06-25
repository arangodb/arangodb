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
#include "Basics/encoding.h"
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
                     std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
                     VPackSlice info)
    : RocksDBIndex(0, collection, StaticStrings::IndexNameTime,
                   attributes, /*unique*/false, /*sparse*/false, RocksDBColumnFamily::time(),
                   /*objectId*/basics::VelocyPackHelper::stringUInt64(info, "objectId"), /*useCache*/false) {
      TRI_ASSERT(_cf == RocksDBColumnFamily::time());
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
    : RocksDBMetaCollection(collection, info), _seriesInfo(info) {
  TRI_ASSERT(_logicalCollection.isAStub() || _objectId != 0);
}

RocksDBTimeseries::RocksDBTimeseries(LogicalCollection& collection,
                                     PhysicalCollection const* physical)
    : RocksDBMetaCollection(collection, this),
      _seriesInfo(static_cast<RocksDBTimeseries const*>(physical)->_seriesInfo) {}

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
  _seriesInfo.toVelocyPack(result);
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

  std::vector<std::vector<arangodb::basics::AttributeName>> attrs;
  attrs.reserve(_seriesInfo.labels.size());
  
  const bool allowExpansion = false;
  for (auto const& label : _seriesInfo.labels) {
    std::vector<arangodb::basics::AttributeName> parsedAttributes;
    TRI_ParseAttributeString(label.name, parsedAttributes, allowExpansion);
    attrs.emplace_back(std::move(parsedAttributes));
  }
  
  VPackSlice slice = VPackSlice::emptyObjectSlice();
  if (indexesSlice.length() >= 1) {
    slice = indexesSlice.at(0);
  }
  
  std::vector<std::shared_ptr<Index>> indexes;
  indexes.emplace_back(std::make_shared<::RocksDBTimeIndex>(_logicalCollection, attrs, slice));
  
//  {
//    READ_LOCKER(guard, _indexesLock);  // link creation needs read-lock too
//    if (indexesSlice.length() == 0 && _indexes.empty()) {
//      engine->indexFactory().fillSystemIndexes(_logicalCollection, indexes);
//    } else {
//      engine->indexFactory().prepareIndexes(_logicalCollection, indexesSlice, indexes);
//    }
//  }
//  TRI_ASSERT(indexes.size() == 1);
//  if (indexes.size() != 1 || indexes[0]->type() != Index::IndexType::TRI_IDX_TYPE_TIMESERIES) {
//    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
//  }

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
  TRI_ASSERT(false);
  return LocalDocumentId();
}

bool RocksDBTimeseries::lookupRevision(transaction::Methods* trx, VPackSlice const& key,
                                       TRI_voc_rid_t& revisionId) const {
  TRI_ASSERT(key.isString());
  revisionId = 0;
  TRI_ASSERT(false);
}

Result RocksDBTimeseries::read(transaction::Methods* trx,
                               arangodb::velocypack::StringRef const& key,
                               ManagedDocumentResult& result, bool /*lock*/) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

// read using a token!
bool RocksDBTimeseries::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result) const {
  result.clear();
  return false;
}

// read using a token!
bool RocksDBTimeseries::readDocumentWithCallback(transaction::Methods* trx,
                                                 LocalDocumentId const& documentId,
                                                 IndexIterator::DocumentCallback const& cb) const {
//  if (documentId.isSet()) {
//    return lookupDocumentVPack(trx, documentId, cb, /*withCache*/true);
//  }
  TRI_ASSERT(false);
  return false;
}

/// @brief new object for insert, computes the hash of the key
Result RocksDBTimeseries::newTimepointForInsert(transaction::Methods* trx,
                                                VPackSlice const& value,
                                                VPackBuilder& builder, bool isRestore,
                                                uint64_t& epoch,
                                                TRI_voc_rid_t& revisionId) const {
  builder.openObject();
  
  auto now = std::chrono::system_clock::now();
  auto epos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
  uint64_t epoch64 = epos.count();
  
  VPackSlice timeSlice = value.get(StaticStrings::TimeString);
  if (!timeSlice.isNone()) {
    TRI_ASSERT(false);
  }
  
  // add system attributes first, in this order:
  // _key, _id, _from, _to, _rev
  
  // _key
  VPackSlice s = value.get(StaticStrings::KeyString);
  if (s.isNone()) {
    TRI_ASSERT(!isRestore);  // need key in case of restore
    auto keyString = _logicalCollection.keyGenerator()->generate();
    
    if (keyString.empty()) {
      return Result(TRI_ERROR_ARANGO_OUT_OF_KEYS);
    }
    
    builder.add(StaticStrings::KeyString, VPackValue(keyString));
  } else {
    return Result(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD, "custom key not supported");
  }
  
  // _id
  uint8_t* p = builder.add(StaticStrings::IdString,
                           VPackValuePair(9ULL, VPackValueType::Custom));
  
  *p++ = 0xf3;  // custom type for _id
  
  if (_isDBServer && !_logicalCollection.system()) {
    // db server in cluster, note: the local collections _statistics,
    // _statisticsRaw and _statistics15 (which are the only system
    // collections)
    // must not be treated as shards but as local collections
    encoding::storeNumber<uint64_t>(p, _logicalCollection.planId(), sizeof(uint64_t));
  } else {
    // local server
    encoding::storeNumber<uint64_t>(p, _logicalCollection.id(), sizeof(uint64_t));
  }
  
  // _rev
  bool handled = false;
  if (isRestore) {
    // copy revision id verbatim
    s = value.get(StaticStrings::RevString);
    if (s.isString()) {
      builder.add(StaticStrings::RevString, s);
      VPackValueLength l;
      char const* p = s.getStringUnchecked(l);
      revisionId = TRI_StringToRid(p, l, false);
      handled = true;
    }
  }
  if (!handled) {
    // temporary buffer for stringifying revision ids
    char ridBuffer[21];
    revisionId = TRI_HybridLogicalClock(); // PhysicalCollection::newRevisionId();
    builder.add(StaticStrings::RevString, TRI_RidToValuePair(revisionId, &ridBuffer[0]));
  }
  
  // _time
  if (!timeSlice.isNone()) {
    // TODO reset epoch64
    builder.add(StaticStrings::TimeString, timeSlice);
  } else {
    builder.add(StaticStrings::TimeString, VPackValue(epoch64));
  }
  
  // add other attributes after the system attributes
  TRI_SanitizeObjectWithEdges(value, builder); // TODO sanitize _time
  
  builder.close();
  return Result();
}

Result RocksDBTimeseries::insert(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& resultMdr,
                                 OperationOptions& options,
                                 bool /*lock*/, KeyLockInfo* /*keyLockInfo*/,
                                 std::function<void()> const& cbDuringLock) {
  TRI_ASSERT(TRI_COL_TYPE_TIMESERIES == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  uint64_t epoch;
  TRI_voc_tick_t revisionId;
  Result res(newTimepointForInsert(trx, slice, *builder.get(),
                                     options.isRestore, epoch, revisionId));
  if (res.fail()) {
    return res;
  }

  VPackSlice newSlice = builder->slice();
//  if (options.overwrite) {
//    return Result(TRI_ERROR_NOT_IMPLEME)
//  }

//  LocalDocumentId const documentId = LocalDocumentId::create();
  LocalDocumentId const documentId = LocalDocumentId::create(epoch);

  RocksDBSavePoint guard(trx, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  auto* state = RocksDBTransactionState::toState(trx);
  state->prepareOperation(_logicalCollection.id(), revisionId, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());

  uint16_t bucketId = _seriesInfo.bucketId(newSlice);
  
  RocksDBKeyLeaser key(trx);
  key->constructTimepoint(_objectId, bucketId, documentId);
  
  trackWaitForSync(trx, options);
  
  rocksdb::Status s;
  if (trx->isSingleOperationTransaction()) {
    TRI_ASSERT(state->rocksdbMethods() == nullptr);
    auto* db = rocksutils::globalRocksDB()->GetRootDB();
    rocksdb::WriteOptions wo;
    if (state->waitForSync()) {
      wo.sync = true;
    }
    s = db->Put(wo, RocksDBColumnFamily::time(), key->string(),
                rocksdb::Slice(newSlice.startAs<char>(),
                static_cast<size_t>(newSlice.byteSize())));
  } else {
    RocksDBMethods* mthds = state->rocksdbMethods();
    // disable indexing in this transaction if we are allowed to
    IndexingDisabler disabler(mthds, !state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED));
    
    TRI_ASSERT(key->containsLocalDocumentId(documentId));
    mthds->PutUntracked(RocksDBColumnFamily::time(), key.ref(),
                        rocksdb::Slice(newSlice.startAs<char>(),
                                       static_cast<size_t>(newSlice.byteSize())));
  }
  
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }
  
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

  return res;
}

Result RocksDBTimeseries::update(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const newSlice,
                                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                                 bool /*lock*/, ManagedDocumentResult& previousMdr) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

Result RocksDBTimeseries::replace(transaction::Methods* trx,
                                  arangodb::velocypack::Slice const newSlice,
                                  ManagedDocumentResult& resultMdr, OperationOptions& options,
                                  bool /*lock*/, ManagedDocumentResult& previousMdr) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

Result RocksDBTimeseries::remove(transaction::Methods& trx, velocypack::Slice slice,
                                 ManagedDocumentResult& previousMdr, OperationOptions& options,
                                 bool /*lock*/, KeyLockInfo* /*keyLockInfo*/,
                                 std::function<void()> const& cbDuringLock) {
  return Result(TRI_ERROR_NOT_IMPLEMENTED);
}

/// @brief return engine-specific figures
void RocksDBTimeseries::figuresSpecific(std::shared_ptr<arangodb::velocypack::Builder>& builder) {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionTimeseries(_objectId);
  rocksdb::Range r(bounds.start(), bounds.end());

  uint64_t out = 0;
  db->GetApproximateSizes(RocksDBColumnFamily::time(), &r, 1, &out,
                          static_cast<uint8_t>(
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));

  builder->add("documentsSize", VPackValue(out));
  builder->add("cacheInUse", VPackValue(false));
  builder->add("cacheSize", VPackValue(0));
  builder->add("cacheUsage", VPackValue(0));
}
