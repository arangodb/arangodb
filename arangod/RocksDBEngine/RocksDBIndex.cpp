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
#include "Cache/TransactionalCache.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
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

// This is the number of distinct elements the index estimator can reliably store
// This correlates directly with the memmory of the estimator:
// memmory == ESTIMATOR_SIZE * 6 bytes

uint64_t const arangodb::RocksDBIndex::ESTIMATOR_SIZE = 4096;

RocksDBIndex::RocksDBIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse, uint64_t objectId, bool useCache)
    : Index(id, collection, attributes, unique, sparse),
      _objectId((objectId != 0) ? objectId : TRI_NewTickServer()),
      _cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()),
      _cache(nullptr),
      _cachePresent(false),
      _useCache(useCache) {
  if (_useCache) {
    createCache();
  } 
}

RocksDBIndex::RocksDBIndex(TRI_idx_iid_t id, LogicalCollection* collection,
                           VPackSlice const& info, bool useCache)
    : Index(id, collection, info),
      _objectId(basics::VelocyPackHelper::stringUInt64(info.get("objectId"))),
      _cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()),
      _cache(nullptr),
      _cachePresent(false),
      _useCache(useCache) {
  if (_objectId == 0) {
    _objectId = TRI_NewTickServer();
  }
  if (_useCache) {
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
  return _cmp;
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

int RocksDBIndex::unload() {
  if (useCache()) {
    //LOG_TOPIC(ERR, Logger::FIXME) << "unload cache";
    disableCache();
    TRI_ASSERT(!_cachePresent);
  }
  return TRI_ERROR_NO_ERROR;
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
  if (!_useCache || _cachePresent) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(_useCache);
  TRI_ASSERT(_cache.get() == nullptr);
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  _cache = CacheManagerFeature::MANAGER->createCache(
      cache::CacheType::Transactional);
  _cachePresent = (_cache.get() != nullptr);
  TRI_ASSERT(_useCache);
}

void RocksDBIndex::disableCache() {
  if (!_cachePresent) {
    return;
  }
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  // must have a cache...
  TRI_ASSERT(_useCache);
  TRI_ASSERT(_cachePresent);
  TRI_ASSERT(_cache.get() != nullptr);
  CacheManagerFeature::MANAGER->destroyCache(_cache);
  _cache.reset();
  _cachePresent = false;
  TRI_ASSERT(_useCache);
}

int RocksDBIndex::drop() {
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
  return TRI_ERROR_NO_ERROR;
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

void RocksDBIndex::truncate(transaction::Methods* trx) {
  RocksDBMethods *mthds = rocksutils::toRocksMethods(trx);
  RocksDBKeyBounds indexBounds = getBounds();

  rocksdb::ReadOptions options = mthds->readOptions();
  rocksdb::Slice end = indexBounds.end();
  options.iterate_upper_bound = &end;


  std::unique_ptr<rocksdb::Iterator> iter = mthds->NewIterator(options);
  iter->Seek(indexBounds.start());

  while (iter->Valid() && _cmp->Compare(iter->key(), end) < 0) {
    Result r = mthds->Delete(iter->key());
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION(r);
    }

    r = postprocessRemove(trx, iter->key(), iter->value());
    if (!r.ok()) {
      THROW_ARANGO_EXCEPTION(r);
    }

    iter->Next();
  }
}

Result RocksDBIndex::postprocessRemove(transaction::Methods* trx,
                                       rocksdb::Slice const& key,
                                      rocksdb::Slice const& value) {
  return {TRI_ERROR_NO_ERROR};
}


// blacklist given key from transactional cache
void RocksDBIndex::blackListKey(char const* data, std::size_t len){
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool blacklisted = false;
    uint64_t attempts = 0;
    while (!blacklisted) {
      blacklisted = _cache->blacklist(data, (uint32_t)len);
      if (attempts++ % 10 == 0) {
        if (_cache->isShutdown()) {
          disableCache();
          break;
        }
      }
    }
  }
}

RocksDBKeyBounds RocksDBIndex::getBounds() const {
  switch (type()) {
    case RocksDBIndex::TRI_IDX_TYPE_PRIMARY_INDEX:
      return RocksDBKeyBounds::PrimaryIndex(objectId());
    case RocksDBIndex::TRI_IDX_TYPE_EDGE_INDEX:
      return RocksDBKeyBounds::EdgeIndex(objectId());
    case RocksDBIndex::TRI_IDX_TYPE_HASH_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_SKIPLIST_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_PERSISTENT_INDEX:
      if (unique()) {
        return RocksDBKeyBounds::UniqueIndex(objectId());
      }
      return RocksDBKeyBounds::IndexEntries(objectId());
    case RocksDBIndex::TRI_IDX_TYPE_FULLTEXT_INDEX:
      return RocksDBKeyBounds::FulltextIndex(objectId());
    case RocksDBIndex::TRI_IDX_TYPE_GEO1_INDEX:
    case RocksDBIndex::TRI_IDX_TYPE_GEO2_INDEX:
      return RocksDBKeyBounds::GeoIndex(objectId());
    case RocksDBIndex::TRI_IDX_TYPE_UNKNOWN:
    default:
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
}
