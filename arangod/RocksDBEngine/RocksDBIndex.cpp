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

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/VelocyPackHelper.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
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
                           uint64_t objectId, uint64_t tempObjectId, bool useCache)
    : Index(id, collection, name, attributes, unique, sparse),
      _cf(cf),
      _cache(nullptr),
      _cacheEnabled(useCache && !collection.system() && CacheManagerFeature::MANAGER != nullptr),
      _objectId(::ensureObjectId(objectId)),
      _tempObjectId(tempObjectId) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamily::definitions());

  if (_cacheEnabled) {
    createCache();
  }

  RocksDBEngine* engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);

  engine->addIndexMapping(_objectId.load(), collection.vocbase().id(),
                          collection.id(), _iid);
}

RocksDBIndex::RocksDBIndex(IndexId id, LogicalCollection& collection,
                           arangodb::velocypack::Slice const& info,
                           rocksdb::ColumnFamilyHandle* cf, bool useCache)
    : Index(id, collection, info),
      _cf(cf),
      _cache(nullptr),
      _cacheEnabled(useCache && !collection.system() && CacheManagerFeature::MANAGER != nullptr),
      _objectId(::ensureObjectId(
          basics::VelocyPackHelper::stringUInt64(info, StaticStrings::ObjectId))),
      _tempObjectId(basics::VelocyPackHelper::stringUInt64(info, StaticStrings::TempObjectId)) {
  TRI_ASSERT(cf != nullptr && cf != RocksDBColumnFamily::definitions());

  if (_cacheEnabled) {
    createCache();
  }

  RocksDBEngine* engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
  engine->addIndexMapping(_objectId.load(), collection.vocbase().id(),
                          collection.id(), _iid);
}

RocksDBIndex::~RocksDBIndex() {
  auto engine = static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE);
  engine->removeIndexMapping(_objectId.load());

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
    builder.add(StaticStrings::TempObjectId,
                VPackValue(std::to_string(_tempObjectId.load())));
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
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  LOG_TOPIC("49e6c", DEBUG, Logger::CACHE) << "Creating index cache";
  _cache = CacheManagerFeature::MANAGER->createCache(cache::CacheType::Transactional);
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBIndex::destroyCache() {
  if (!_cache) {
    return;
  }
  TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
  // must have a cache...
  TRI_ASSERT(_cache.get() != nullptr);
  LOG_TOPIC("b5d85", DEBUG, Logger::CACHE) << "Destroying index cache";
  CacheManagerFeature::MANAGER->destroyCache(_cache);
  _cache.reset();
}

Result RocksDBIndex::drop() {
  auto* coll = toRocksDBCollection(_collection);
  // edge index needs to be dropped with prefixSameAsStart = false
  // otherwise full index scan will not work
  bool const prefixSameAsStart = this->type() != Index::TRI_IDX_TYPE_EDGE_INDEX;
  bool const useRangeDelete = coll->meta().numberDocuments() >= 32 * 1024;

  arangodb::Result r =
      rocksutils::removeLargeRange(rocksutils::globalRocksDB(), this->getBounds(),
                                   prefixSameAsStart, useRangeDelete);

  // Try to drop the cache as well.
  if (_cache) {
    try {
      TRI_ASSERT(CacheManagerFeature::MANAGER != nullptr);
      CacheManagerFeature::MANAGER->destroyCache(_cache);
      // Reset flag
      _cache.reset();
    } catch (...) {
    }
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // check if documents have been deleted
  size_t numDocs = rocksutils::countKeyRange(rocksutils::globalRocksDB(),
                                             this->getBounds(), prefixSameAsStart);
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

Result RocksDBIndex::update(transaction::Methods& trx, RocksDBMethods* mthd,
                            LocalDocumentId const& oldDocumentId,
                            velocypack::Slice const& oldDoc,
                            LocalDocumentId const& newDocumentId,
                            velocypack::Slice const& newDoc, Index::OperationMode mode) {
  // It is illegal to call this method on the primary index
  // RocksDBPrimaryIndex must override this method accordingly
  TRI_ASSERT(type() != TRI_IDX_TYPE_PRIMARY_INDEX);
  
  /// only if the insert needs to see the changes of the update, enable indexing:
  IndexingEnabler enabler(mthd, mthd->isIndexingDisabled() && hasExpansion() && unique());
  
  TRI_ASSERT((hasExpansion() && unique()) ? !mthd->isIndexingDisabled() : true);
  
  Result res = remove(trx, mthd, oldDocumentId, oldDoc, mode);
  if (!res.ok()) {
    return res;
  }
  OperationOptions options;
  options.indexOperationMode = mode;
  return insert(trx, mthd, newDocumentId, newDoc, options);
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
void RocksDBIndex::compact() {
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  rocksdb::CompactRangeOptions opts;
  if (_cf != RocksDBColumnFamily::invalid()) {
    RocksDBKeyBounds bounds = this->getBounds();
    TRI_ASSERT(_cf == bounds.columnFamily());
    rocksdb::Slice b = bounds.start(), e = bounds.end();
    db->CompactRange(opts, _cf, &b, &e);
  }
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

Result RocksDBIndex::setObjectIds(std::uint64_t plannedObjectId,
                                  std::uint64_t plannedTempObjectId) {
  Result res;
  auto& server = _collection.vocbase().server();
  auto& selector = server.getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();

  if (plannedObjectId == _objectId.load() &&
      plannedTempObjectId != _tempObjectId.load()) {
    TRI_ASSERT(_tempObjectId.load() == 0 || plannedTempObjectId == 0);
    // just temp id has changed
    std::uint64_t oldId = _tempObjectId.load();
    _tempObjectId.store(plannedTempObjectId);
    if (oldId != 0) {
      try {
        RocksDBKeyBounds bounds = getBounds(type(), oldId, unique());
        return rocksutils::removeLargeRange(engine.db(), bounds,
                                            this->type() != Index::TRI_IDX_TYPE_EDGE_INDEX,
                                            true);
      } catch (arangodb::basics::Exception& ex) {
        if (ex.code() != TRI_ERROR_NOT_IMPLEMENTED) {  // in case we hit an
                                                       // IResearchLink, etc.
          throw ex;
        }
      }
    }
  } else if (plannedTempObjectId != _tempObjectId.load()) {
    TRI_ASSERT(plannedObjectId != _objectId.load());
    TRI_ASSERT(plannedObjectId != 0);
    TRI_ASSERT(plannedObjectId = _tempObjectId.load());
    // swapping in new range
    std::uint64_t oldId = _objectId.load();
    _tempObjectId.store(plannedTempObjectId);
    _objectId.store(plannedObjectId);
    engine.addIndexMapping(_objectId.load(), _collection.vocbase().id(),
                           _collection.id(), id());
    engine.removeIndexMapping(oldId);
  }

  return res;
}
