////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/VelocyPackHelper.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <rocksdb/comparator.h>

using namespace arangodb;
using namespace arangodb::rocksutils;

// This is the number of distinct elements the index estimator can reliably
// store
// This correlates directly with the memory of the estimator:
// memory == ESTIMATOR_SIZE * 6 bytes

uint64_t const arangodb::RocksDBIndex::ESTIMATOR_SIZE = 4096;

RocksDBIndex::RocksDBIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse, rocksdb::ColumnFamilyHandle* cf,
    uint64_t objectId, bool useCache)
    : Index(id, collection, attributes, unique, sparse),
      _objectId((objectId != 0) ? objectId : TRI_NewTickServer()),
      _cf(cf),
      _cache(nullptr),
      _cachePresent(false),
      _cacheEnabled(useCache && !collection->isSystem()) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamily::definitions());
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBIndex::RocksDBIndex(TRI_idx_iid_t id, LogicalCollection* collection,
                           VPackSlice const& info,
                           rocksdb::ColumnFamilyHandle* cf, bool useCache)
    : Index(id, collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info.get("objectId"))),
      _cf(cf),
      _cache(nullptr),
      _cachePresent(false),
      _cacheEnabled(useCache && !collection->isSystem()) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamily::definitions());

  if (_objectId == 0) {
    _objectId = TRI_NewTickServer();
  }
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBIndex::~RocksDBIndex() {
  if (useCache()) {
    try {
      TRI_ASSERT(_cache != nullptr);
      TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
      CacheManagerFeature::MANAGER->destroyCache(_cache);
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
  builder.add("cacheInUse", VPackValue(useCache()));
  if (useCache()) {
    builder.add("cacheSize", VPackValue(_cache->size()));
    auto hitRates = _cache->hitRates();
    double rate = hitRates.first;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheLifeTimeHitRate", VPackValue(rate));
    rate = hitRates.second;
    rate = std::isnan(rate) ? 0.0 : rate;
    builder.add("cacheWindowedHitRate", VPackValue(rate));
  } else {
    builder.add("cacheSize", VPackValue(0));
  }
}

void RocksDBIndex::load() {
  if (_cacheEnabled) {
    createCache();
    TRI_ASSERT(_cachePresent);
  }
}

void RocksDBIndex::unload() {
  if (useCache()) {
    // LOG_TOPIC(ERR, Logger::FIXME) << "unload cache";
    destroyCache();
    TRI_ASSERT(!_cachePresent);
  }
}

/// @brief return a VelocyPack representation of the index
void RocksDBIndex::toVelocyPack(VPackBuilder& builder, bool withFigures,
                                bool forPersistence) const {
  Index::toVelocyPack(builder, withFigures, forPersistence);
  if (forPersistence) {
    // If we store it, it cannot be 0
    TRI_ASSERT(_objectId != 0);
    builder.add("objectId", VPackValue(std::to_string(_objectId)));
  }
}

void RocksDBIndex::createCache() {
  if (!_cacheEnabled || _cachePresent ||
      ServerState::instance()->isCoordinator()) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(!_collection->isSystem() &&
             !ServerState::instance()->isCoordinator());
  TRI_ASSERT(_cache.get() == nullptr);
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  _cache = CacheManagerFeature::MANAGER->createCache(
      cache::CacheType::Transactional);
  _cachePresent = (_cache.get() != nullptr);
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBIndex::destroyCache() {
  if (!_cachePresent) {
    return;
  }
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  // must have a cache...
  TRI_ASSERT(_cacheEnabled);
  TRI_ASSERT(_cachePresent);
  TRI_ASSERT(_cache.get() != nullptr);
  CacheManagerFeature::MANAGER->destroyCache(_cache);
  _cache.reset();
  _cachePresent = false;
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBIndex::serializeEstimate(std::string&) const {
  // All indexes that do not have an estimator do not serialize anything.
}

bool RocksDBIndex::deserializeEstimate(RocksDBCounterManager*) {
  // All indexes that do not have an estimator do not deserialize anything.
  // So the estimate is always recreatable.
  // We do not advance anything here.
  return true;
}

void RocksDBIndex::recalculateEstimates() {
  // Nothing to do.
  return;
}

int RocksDBIndex::drop() {
  // edge index needs to be dropped with prefix_same_as_start = false
  // otherwise full index scan will not work
  bool prefix_same_as_start = this->type() != Index::TRI_IDX_TYPE_EDGE_INDEX;
  arangodb::Result r = rocksutils::removeLargeRange(
    rocksutils::globalRocksDB(), this->getBounds(), prefix_same_as_start);
  
  // Try to drop the cache as well.
  if (_cachePresent) {
    try {
      TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
      CacheManagerFeature::MANAGER->destroyCache(_cache);
      // Reset flag
      _cache.reset();
      _cachePresent = false;
    } catch (...) {
    }
  }
  
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  //check if documents have been deleted
  size_t numDocs = rocksutils::countKeyRange(rocksutils::globalRocksDB(),
                                             this->getBounds(), prefix_same_as_start);
  if (numDocs > 0) {
    
    std::string errorMsg("deletion check in index drop failed - not all documents in the index have been deleted. remaining: ");
    errorMsg.append(std::to_string(numDocs));
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, errorMsg);
  }
#endif
  
  return r.errorNumber();
}

Result RocksDBIndex::updateInternal(transaction::Methods* trx, RocksDBMethods* mthd,
                      TRI_voc_rid_t oldRevision,
                      arangodb::velocypack::Slice const& oldDoc,
                      TRI_voc_rid_t newRevision,
                                    arangodb::velocypack::Slice const& newDoc) {
  Result res = removeInternal(trx, mthd, oldRevision, oldDoc);
  if (!res.ok()) {
      return res;
  }
  return insertInternal(trx, mthd, newRevision, newDoc);
}

void RocksDBIndex::truncate(transaction::Methods* trx) {
  auto* mthds = RocksDBTransactionState::toMethods(trx);
  RocksDBKeyBounds indexBounds = getBounds(type(), _objectId, _unique);

  rocksdb::ReadOptions options = mthds->readOptions();
  rocksdb::Slice end = indexBounds.end();
  rocksdb::Comparator const* cmp = this->comparator();
  options.iterate_upper_bound = &end;
  if (type() == RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX) {
    options.prefix_same_as_start = false;
    options.total_order_seek = true;
  }
  options.verify_checksums = false;
  options.fill_cache = false;

  std::unique_ptr<rocksdb::Iterator> iter = mthds->NewIterator(options, _cf);
  iter->Seek(indexBounds.start());

  while (iter->Valid() && cmp->Compare(iter->key(), end) < 0) {
    TRI_ASSERT(_objectId == RocksDBKey::objectId(iter->key()));

    Result r = mthds->Delete(_cf, RocksDBKey(iter->key()));
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION(r);
    }

    r = postprocessRemove(trx, iter->key(), iter->value());
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION(r);
    }

    iter->Next();
  }
  
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  //check if index entries have been deleted
  if (type() != TRI_IDX_TYPE_GEO1_INDEX && type() != TRI_IDX_TYPE_GEO2_INDEX) {
    if (mthds->countInBounds(getBounds(), true)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "deletion check in collection truncate "
                                     "failed - not all documents in an index "
                                     "have been deleted");
    }
  }
#endif
}

/// @brief return the memory usage of the index
size_t RocksDBIndex::memory() const {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
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
void RocksDBIndex::cleanup() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  RocksDBKeyBounds bounds = this->getBounds();
  TRI_ASSERT(_cf == bounds.columnFamily());
  rocksdb::Slice b = bounds.start(), e = bounds.end();
  db->CompactRange(opts, _cf, &b, &e);
}

Result RocksDBIndex::postprocessRemove(transaction::Methods* trx,
                                       rocksdb::Slice const& key,
                                       rocksdb::Slice const& value) {
  return {TRI_ERROR_NO_ERROR};
}

// blacklist given key from transactional cache
void RocksDBIndex::blackListKey(char const* data, std::size_t len) {
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool blacklisted = false;
    while (!blacklisted) {
      auto status = _cache->blacklist(data, static_cast<uint32_t>(len));
      if (status.ok()) {
        blacklisted = true;
      } else if (status.errorNumber() == TRI_ERROR_SHUTTING_DOWN) {
        destroyCache();
        break;
      }
    }
  }
}

RocksDBKeyBounds RocksDBIndex::getBounds(Index::IndexType type,
                                         uint64_t objectId, bool unique) {
  switch (type) {
    case RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
      return RocksDBKeyBounds::PrimaryIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
      return RocksDBKeyBounds::EdgeIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
      if (unique) {
        return RocksDBKeyBounds::UniqueVPackIndex(objectId);
      }
      return RocksDBKeyBounds::VPackIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_FULLTEXT_INDEX:
      return RocksDBKeyBounds::FulltextIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_GEO1_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_GEO2_INDEX:
      return RocksDBKeyBounds::GeoIndex(objectId);
    case RocksDBIndex::TRI_IDX_TYPE_UNKNOWN:
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}
