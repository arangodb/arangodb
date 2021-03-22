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

#include "RocksDBIndex.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBCuckooIndexEstimator.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/comparator.h>
#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

constexpr uint64_t arangodb::RocksDBIndex::ESTIMATOR_SIZE;

namespace {
inline uint64_t ensureObjectId(uint64_t oid) {
  return (oid != 0) ? oid : TRI_NewTickServer();
}
}  // namespace

RocksDBIndex::RocksDBIndex(IndexId id, LogicalCollection& collection,
                           std::string const& name,
                           std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
                           bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
                           uint64_t objectId, bool useCache)
    : Index(id, collection, name, attributes, unique, sparse),
      _cf(cf),
      _cache(nullptr),
      _cacheEnabled(
          useCache && !collection.system() &&
          collection.vocbase().server().getFeature<CacheManagerFeature>().manager() != nullptr),
      _objectId(::ensureObjectId(objectId)) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamilyManager::get(
                                        RocksDBColumnFamilyManager::Family::Definitions));

  if (_cacheEnabled) {
    createCache();
  }

  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();

  engine.addIndexMapping(_objectId.load(), collection.vocbase().id(), collection.id(), _iid);
}

RocksDBIndex::RocksDBIndex(IndexId id, LogicalCollection& collection,
                           arangodb::velocypack::Slice const& info,
                           rocksdb::ColumnFamilyHandle* cf, bool useCache)
    : Index(id, collection, info),
      _cf(cf),
      _cache(nullptr),
      _cacheEnabled(
          useCache && !collection.system() &&
          collection.vocbase().server().getFeature<CacheManagerFeature>().manager() != nullptr),
      _objectId(::ensureObjectId(
          basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId))) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamilyManager::get(
                                        RocksDBColumnFamilyManager::Family::Definitions));

  if (_cacheEnabled) {
    createCache();
  }

  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  engine.addIndexMapping(_objectId.load(), collection.vocbase().id(), collection.id(), _iid);
}

RocksDBIndex::~RocksDBIndex() {
  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  engine.removeIndexMapping(_objectId.load());

  if (useCache()) {
    try {
      TRI_ASSERT(_cache != nullptr);
      auto* manager =
          _collection.vocbase().server().getFeature<CacheManagerFeature>().manager();
      TRI_ASSERT(manager != nullptr);
      manager->destroyCache(_cache);
    } catch (...) {
    }
  }
}

rocksdb::Comparator const* RocksDBIndex::comparator() const {
  return _cf->GetComparator();
}

void RocksDBIndex::toVelocyPackFigures(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  Index::toVelocyPackFigures(builder);
  bool cacheInUse = useCache();
  builder.add("cacheInUse", VPackValue(cacheInUse));
  if (cacheInUse) {
    builder.add("cacheSize", VPackValue(_cache->size()));
    builder.add("cacheUsage", VPackValue(_cache->usage()));
    auto hitRates = _cache->hitRates();
    double rate = hitRates.first;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheLifeTimeHitRate", VPackValue(rate));
    rate = hitRates.second;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheWindowedHitRate", VPackValue(rate));
  } else {
    builder.add("cacheSize", VPackValue(0));
    builder.add("cacheUsage", VPackValue(0));
  }
}

void RocksDBIndex::load() {
  if (_cacheEnabled) {
    createCache();
  }
}

void RocksDBIndex::unload() {
  if (useCache()) {
    destroyCache();
    TRI_ASSERT(_cache.get() == nullptr);
  }
}

/// @brief return a VelocyPack representation of the index
void RocksDBIndex::toVelocyPack(VPackBuilder& builder,
                                std::underlying_type<Serialize>::type flags) const {
  Index::toVelocyPack(builder, flags);
  if (Index::hasFlag(flags, Index::Serialize::Internals)) {
    // If we store it, it cannot be 0
    TRI_ASSERT(_objectId.load() != 0);
    builder.add(StaticStrings::ObjectId, VPackValue(std::to_string(_objectId.load())));
  }
  builder.add(arangodb::StaticStrings::IndexUnique, VPackValue(unique()));
  builder.add(arangodb::StaticStrings::IndexSparse, VPackValue(sparse()));
}

void RocksDBIndex::createCache() {
  if (!_cacheEnabled || _cache != nullptr || _collection.isAStub() ||
      ServerState::instance()->isCoordinator()) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(!_collection.system() && !ServerState::instance()->isCoordinator());
  TRI_ASSERT(_cache.get() == nullptr);
  auto* manager =
      _collection.vocbase().server().getFeature<CacheManagerFeature>().manager();
  TRI_ASSERT(manager != nullptr);
  LOG_TOPIC("49e6c", DEBUG, Logger::CACHE) << "Creating index cache";
  _cache = manager->createCache(cache::CacheType::Transactional);
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBIndex::destroyCache() {
  if (!_cache) {
    return;
  }
  auto* manager =
      _collection.vocbase().server().getFeature<CacheManagerFeature>().manager();
  TRI_ASSERT(manager != nullptr);
  // must have a cache...
  TRI_ASSERT(_cache.get() != nullptr);
  LOG_TOPIC("b5d85", DEBUG, Logger::CACHE) << "Destroying index cache";
  manager->destroyCache(_cache);
  _cache.reset();
}

Result RocksDBIndex::drop() {
  auto* coll = toRocksDBCollection(_collection);
  // edge index needs to be dropped with prefixSameAsStart = false
  // otherwise full index scan will not work
  bool const prefixSameAsStart = this->type() != Index::TRI_IDX_TYPE_EDGE_INDEX;
  bool const useRangeDelete = coll->meta().numberDocuments() >= 32 * 1024;

  RocksDBEngine& engine =
      _collection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  arangodb::Result r = rocksutils::removeLargeRange(engine.db(), this->getBounds(),
                                                    prefixSameAsStart, useRangeDelete);

  // Try to drop the cache as well.
  if (_cache) {
    try {
      auto* manager =
          _collection.vocbase().server().getFeature<CacheManagerFeature>().manager();
      TRI_ASSERT(manager != nullptr);
      manager->destroyCache(_cache);
      // Reset flag
      _cache.reset();
    } catch (...) {
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs =
      rocksutils::countKeyRange(engine.db(), this->getBounds(), prefixSameAsStart);
  if (numDocs > 0) {
    std::string errorMsg(
        "deletion check in index drop failed - not all documents in the index "
        "have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif

  return r;
}

void RocksDBIndex::afterTruncate(TRI_voc_tick_t, arangodb::transaction::Methods*) {
  // simply drop the cache and re-create it
  if (_cacheEnabled) {
    destroyCache();
    createCache();
    TRI_ASSERT(_cache.get() != nullptr);
  }
}  
  
/// performs a preflight check for an insert operation, not carrying out any
/// modifications to the index.
/// the default implementation does nothing. indexes can override this and
/// perform useful checks (uniqueness checks etc.) here
Result RocksDBIndex::checkInsert(transaction::Methods& /*trx*/, 
                                 RocksDBMethods* /*methods*/,
                                 LocalDocumentId const& /*documentId*/,
                                 arangodb::velocypack::Slice /*doc*/,
                                 OperationOptions const& /*options*/) {
  // default implementation does nothing - derived indexes can override this!
  return {};
}

/// performs a preflight check for an update/replace operation, not carrying out any
/// modifications to the index.
/// the default implementation does nothing. indexes can override this and
/// perform useful checks (uniqueness checks etc.) here
Result RocksDBIndex::checkReplace(transaction::Methods& /*trx*/, 
                                  RocksDBMethods* /*methods*/,
                                  LocalDocumentId const& /*documentId*/,
                                  arangodb::velocypack::Slice /*doc*/,
                                  OperationOptions const& /*options*/) {
  // default implementation does nothing - derived indexes can override this!
  return {};
}

Result RocksDBIndex::update(transaction::Methods& trx, RocksDBMethods* mthd,
                            LocalDocumentId const& oldDocumentId,
                            velocypack::Slice oldDoc,
                            LocalDocumentId const& newDocumentId,
                            velocypack::Slice newDoc,
                            OperationOptions const& options) {
  // It is illegal to call this method on the primary index
  // RocksDBPrimaryIndex must override this method accordingly
  TRI_ASSERT(type() != TRI_IDX_TYPE_PRIMARY_INDEX);
  
  /// only if the insert needs to see the changes of the update, enable indexing:
  IndexingEnabler enabler(mthd, mthd->isIndexingDisabled() && hasExpansion() && unique());
  
  TRI_ASSERT((hasExpansion() && unique()) ? !mthd->isIndexingDisabled() : true);
  
  Result res = remove(trx, mthd, oldDocumentId, oldDoc);
  if (!res.ok()) {
    return res;
  }
  return insert(trx, mthd, newDocumentId, newDoc, options);
}

/// @brief return the memory usage of the index
size_t RocksDBIndex::memory() const {
  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  RocksDBKeyBounds bounds = getBounds();
  TRI_ASSERT(_cf == bounds.columnFamily());
  rocksdb::Range r(bounds.start(), bounds.end());
  uint64_t out;
  db->GetApproximateSizes(_cf, &r, 1, &out,
                          static_cast<uint8_t>(
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                              rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));
  return static_cast<size_t>(out);
}

/// compact the index, should reduce read amplification
void RocksDBIndex::compact() {
  auto& selector = _collection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  if (_cf != RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Invalid)) {
    engine.compactRange(getBounds());
  }
}

// banish given key from transactional cache
void RocksDBIndex::invalidateCacheEntry(char const* data, std::size_t len) {
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool banished = false;
    while (!banished) {
      auto status = _cache->banish(data, static_cast<uint32_t>(len));
      if (status.ok()) {
        banished = true;
      } else if (status.errorNumber() == TRI_ERROR_SHUTTING_DOWN) {
        destroyCache();
        break;
      }
    }
  }
}
  
/// @brief get index estimator, optional
RocksDBCuckooIndexEstimatorType* RocksDBIndex::estimator() { 
  return nullptr; 
}

void RocksDBIndex::setEstimator(std::unique_ptr<RocksDBCuckooIndexEstimatorType>) {}

void RocksDBIndex::recalculateEstimates() {}

RocksDBKeyBounds RocksDBIndex::getBounds(Index::IndexType type, uint64_t objectId, bool unique) {
  switch (type) {
    case RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
      return RocksDBKeyBounds::PrimaryIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
      return RocksDBKeyBounds::EdgeIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_TTL_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
      if (unique) {
        return RocksDBKeyBounds::UniqueVPackIndex(objectId, false);
      }
      return RocksDBKeyBounds::VPackIndex(objectId, false);
    case RocksDBIndex::TRI_IDX_TYPE_FULLTEXT_INDEX:
      return RocksDBKeyBounds::FulltextIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_GEO1_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_GEO2_INDEX:
      return RocksDBKeyBounds::LegacyGeoIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_GEO_INDEX:
      return RocksDBKeyBounds::GeoIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_IRESEARCH_LINK:
      return RocksDBKeyBounds::DatabaseViews(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_UNKNOWN:
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}
