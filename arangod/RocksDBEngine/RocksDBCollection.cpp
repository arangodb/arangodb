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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBCollection.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/PlanCache.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/RecursiveLocker.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/debugging.h"
#include "Basics/hashes.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "Cache/TransactionalCache.h"
#include "Cluster/ClusterMethods.h"
#include "Indexes/Index.h"
#include "Indexes/IndexIterator.h"
#include "Random/RandomGenerator.h"
#include "RestServer/DatabaseFeature.h"
#include "RestServer/MetricsFeature.h"
#include "RocksDBEngine/RocksDBBuilderIndex.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBReplicationIterator.h"
#include "RocksDBEngine/RocksDBReplicationManager.h"
#include "RocksDBEngine/RocksDBSavePoint.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "RocksDBEngine/RocksDBTransactionMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Hints.h"
#include "Transaction/StandaloneContext.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Events.h"
#include "Utils/OperationOptions.h"
#include "VocBase/Identifiers/LocalDocumentId.h"
#include "VocBase/Identifiers/RevisionId.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/ticks.h"
#include "VocBase/voc-types.h"

#include <rocksdb/utilities/transaction.h>
#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

namespace {
// number of write operations in transactions after which we will start
// doing preflight checks before every document insert or update/replace.
// the rationale is that if we already have a lot of operations accumulated
// in our transaction's WriteBatch, every rollback due to a unique constraint
// violation will be prohibitively expensive for larger WriteBatch sizes.
// so instead of performing an insert, update/replace directly, we first
// check for uniqueness violations and instantly abort if there are any,
// without having modified the transaction's WriteBatch for the failed
// operation. We can thus avoid the costly RollbackToSavePoint() call here.
// we don't do this preflight check for smaller batches though.
constexpr size_t preflightThreshold = 100;

arangodb::LocalDocumentId generateDocumentId(arangodb::LogicalCollection const& collection,
                                             arangodb::RevisionId revisionId) {
  bool useRev = collection.usesRevisionsAsDocumentIds();
  return useRev ? arangodb::LocalDocumentId::create(revisionId)
                : arangodb::LocalDocumentId::create();
}

template <typename F>
void reverseIdxOps(arangodb::PhysicalCollection::IndexContainerType const& indexes,
                   arangodb::PhysicalCollection::IndexContainerType::const_iterator& it,
                   F&& op) {
  while (it != indexes.begin()) {
    it--;
    auto* rIdx = static_cast<arangodb::RocksDBIndex*>(it->get());
    if (rIdx->needsReversal()) {
      if (std::forward<F>(op)(rIdx).fail()) {
        // best effort for reverse failed. Let`s trigger full rollback
        // or we will end up with inconsistent storage and indexes
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Failed to reverse index operation.");
      }
    }
  }
}

/// @brief remove an index from a container, by id
bool removeIndex(arangodb::PhysicalCollection::IndexContainerType& indexes,
                 arangodb::IndexId id) {
  auto it = indexes.begin();
  while (it != indexes.end()) {
    if ((*it)->id() == id) {
      indexes.erase(it);
      return true;
    }
    ++it;
  }
  return false;
}

/// @brief helper RAII base class to count and time-track a CRUD operation
struct TimeTracker {
  TimeTracker(TimeTracker const&) = delete;
  TimeTracker& operator=(TimeTracker const&) = delete;

  explicit TimeTracker(arangodb::TransactionStatistics const& statistics,
                       std::optional<std::reference_wrapper<Histogram<log_scale_t<float>>>>& histogram) noexcept
      : statistics(statistics), histogram(histogram) {
    if (statistics._exportReadWriteMetrics) {
      // time measurement is not free. only do it if metrics are enabled
      start = getTime();
    }
  }

  ~TimeTracker() {
    if (statistics._exportReadWriteMetrics) {
      // metrics collection is not free. only do it if metrics are enabled
      // unit is seconds here
      TRI_ASSERT(histogram.has_value());
      histogram->get().count(std::chrono::duration<float>(getTime() - start).count());
    }
  }

  std::chrono::time_point<std::chrono::steady_clock> getTime() const noexcept {
    TRI_ASSERT(statistics._exportReadWriteMetrics);
    return std::chrono::steady_clock::now();
  }

  arangodb::TransactionStatistics const& statistics;
  std::optional<std::reference_wrapper<Histogram<log_scale_t<float>>>>& histogram;
  std::chrono::time_point<std::chrono::steady_clock> start;
};

/// @brief helper RAII class to count and time-track a CRUD read operation
struct ReadTimeTracker : public TimeTracker {
  ReadTimeTracker(std::optional<std::reference_wrapper<Histogram<log_scale_t<float>>>>& histogram,
                  arangodb::TransactionStatistics& statistics) noexcept
      : TimeTracker(statistics, histogram) {}
};

/// @brief helper RAII class to count and time-track CRUD write operations
struct WriteTimeTracker : public TimeTracker {
  WriteTimeTracker(std::optional<std::reference_wrapper<Histogram<log_scale_t<float>>>>& histogram,
                   arangodb::TransactionStatistics& statistics,
                   arangodb::OperationOptions const& options) noexcept
      : TimeTracker(statistics, histogram) {
    if (statistics._exportReadWriteMetrics) {
      // metrics collection is not free. only track writes if metrics are enabled
      if (options.isSynchronousReplicationFrom.empty()) {
        ++(statistics._numWrites->get());
      } else {
        ++(statistics._numWritesReplication->get());
      }
    }
  }
};

/// @brief helper RAII class to count and time-track truncate operations
struct TruncateTimeTracker : public TimeTracker {
  TruncateTimeTracker(std::optional<std::reference_wrapper<Histogram<log_scale_t<float>>>>& histogram,
                      arangodb::TransactionStatistics& statistics,
                      arangodb::OperationOptions const& options) noexcept
      : TimeTracker(statistics, histogram) {
    if (statistics._exportReadWriteMetrics) {
      // metrics collection is not free. only track truncates if metrics are enabled
      if (options.isSynchronousReplicationFrom.empty()) {
        ++(statistics._numTruncates->get());
      } else {
        ++(statistics._numTruncatesReplication->get());
      }
    }
  }
};

void reportPrimaryIndexInconsistency(arangodb::Result const& res,
                                     arangodb::velocypack::StringRef const& key,
                                     arangodb::LocalDocumentId const& rev,
                                     arangodb::LogicalCollection const& collection) {
  if (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
    // Scandal! A primary index entry is pointing to nowhere! Let's report
    // this to the authorities immediately:
    LOG_TOPIC("42536", ERR, arangodb::Logger::ENGINES)
        << "Found primary index entry for which there is no actual "
           "document: collection="
        << collection.name() << ", _key=" << key << ", _rev=" << rev.id();
    TRI_ASSERT(false);
  }
}

}  // namespace

namespace arangodb {

RocksDBCollection::RocksDBCollection(LogicalCollection& collection,
                                     arangodb::velocypack::Slice const& info)
    : RocksDBMetaCollection(collection, info),
      _primaryIndex(nullptr),
      _cacheEnabled(
          !collection.system() &&
          basics::VelocyPackHelper::getBooleanValue(info, StaticStrings::CacheEnabled, false) &&
          collection.vocbase().server().getFeature<CacheManagerFeature>().manager() != nullptr),
      _numIndexCreations(0),
      _statistics(collection.vocbase()
                      .server()
                      .getFeature<MetricsFeature>()
                      .serverStatistics()
                      ._transactionsStatistics) {
  TRI_ASSERT(_logicalCollection.isAStub() || objectId() != 0);
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBCollection::RocksDBCollection(LogicalCollection& collection,
                                     PhysicalCollection const* physical)
    : RocksDBMetaCollection(collection, VPackSlice::emptyObjectSlice()),
      _primaryIndex(nullptr),
      _cacheEnabled(
          static_cast<RocksDBCollection const*>(physical)->_cacheEnabled &&
          collection.vocbase().server().getFeature<CacheManagerFeature>().manager() != nullptr),
      _numIndexCreations(0),
      _statistics(collection.vocbase()
                      .server()
                      .getFeature<MetricsFeature>()
                      .serverStatistics()
                      ._transactionsStatistics) {
  if (_cacheEnabled) {
    createCache();
  }
}

RocksDBCollection::~RocksDBCollection() {
  if (useCache()) {
    try {
      destroyCache();
    } catch (...) {
    }
  }
}

Result RocksDBCollection::updateProperties(VPackSlice const& slice, bool doSync) {
  auto isSys = _logicalCollection.system();

  _cacheEnabled =
      !isSys &&
      basics::VelocyPackHelper::getBooleanValue(slice, StaticStrings::CacheEnabled, _cacheEnabled) &&
      _logicalCollection.vocbase().server().getFeature<CacheManagerFeature>().manager() != nullptr;
  primaryIndex()->setCacheEnabled(_cacheEnabled);

  if (_cacheEnabled) {
    createCache();
    primaryIndex()->createCache();
  } else {
    // will do nothing if cache is not present
    destroyCache();
    primaryIndex()->destroyCache();
    TRI_ASSERT(_cache.get() == nullptr);
  }

  // nothing else to do
  return {};
}

PhysicalCollection* RocksDBCollection::clone(LogicalCollection& logical) const {
  return new RocksDBCollection(logical, this);
}

/// @brief export properties
void RocksDBCollection::getPropertiesVPack(velocypack::Builder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add(StaticStrings::ObjectId, VPackValue(std::to_string(objectId())));
  result.add(StaticStrings::CacheEnabled, VPackValue(_cacheEnabled));
  TRI_ASSERT(result.isOpenObject());
}

/// @brief closes an open collection
ErrorCode RocksDBCollection::close() {
  RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
  for (auto it : _indexes) {
    it->unload();
  }
  return TRI_ERROR_NO_ERROR;
}

/// return bounds for all documents
RocksDBKeyBounds RocksDBCollection::bounds() const {
  return RocksDBKeyBounds::CollectionDocuments(objectId());
}

void RocksDBCollection::prepareIndexes(arangodb::velocypack::Slice indexesSlice) {
  TRI_ASSERT(indexesSlice.isArray());

  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  std::vector<std::shared_ptr<Index>> indexes;
  {
    // link creation needs read-lock too
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
    if (indexesSlice.length() == 0 && _indexes.empty()) {
      engine.indexFactory().fillSystemIndexes(_logicalCollection, indexes);
    } else {
      engine.indexFactory().prepareIndexes(_logicalCollection, indexesSlice, indexes);
    }
  }

  RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
  TRI_ASSERT(_indexes.empty());
  for (std::shared_ptr<Index>& idx : indexes) {
    TRI_ASSERT(idx != nullptr);
    auto const id = idx->id();
    for (auto const& it : _indexes) {
      TRI_ASSERT(it != nullptr);
      if (it->id() == id) {  // index is there twice
        idx.reset();
        break;
      }
    }

    if (idx) {
      TRI_UpdateTickServer(static_cast<TRI_voc_tick_t>(id.id()));
      _indexes.emplace(idx);
      if (idx->type() == Index::TRI_IDX_TYPE_PRIMARY_INDEX) {
        TRI_ASSERT(idx->id().isPrimary());
        _primaryIndex = static_cast<RocksDBPrimaryIndex*>(idx.get());
      }
    }
  }

  auto it = _indexes.cbegin();
  if ((*it)->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX ||
      (TRI_COL_TYPE_EDGE == _logicalCollection.type() &&
       (_indexes.size() < 3 ||
        ((*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX ||
         (*++it)->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX)))) {
    std::string msg =
        "got invalid indexes for collection '" + _logicalCollection.name() + "'";
    LOG_TOPIC("0ef34", ERR, arangodb::Logger::ENGINES) << msg;
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    for (auto it : _indexes) {
      LOG_TOPIC("19e0b", ERR, arangodb::Logger::ENGINES) << "- " << it->context();
    }
#endif
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
  }

  TRI_ASSERT(!_indexes.empty());
}

std::shared_ptr<Index> RocksDBCollection::createIndex(VPackSlice const& info,
                                                      bool restore, bool& created) {
  TRI_ASSERT(info.isObject());

  // Step 0. Lock all the things
  TRI_vocbase_t& vocbase = _logicalCollection.vocbase();
  if (!vocbase.use()) {  // someone dropped the database
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }

  std::unique_ptr<CollectionGuard> guard;
  try {
    guard = std::make_unique<CollectionGuard>(&vocbase, _logicalCollection.id());
  } catch (...) {
    vocbase.release();
    throw;
  }

  _numIndexCreations.fetch_add(1, std::memory_order_release);
  auto colGuard = scopeGuard([&]() noexcept {
    _numIndexCreations.fetch_sub(1, std::memory_order_release);
    vocbase.release();
  });

  READ_LOCKER(inventoryLocker, vocbase._inventoryLock);

  RocksDBBuilderIndex::Locker locker(this);
  if (!locker.lock()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_LOCK_TIMEOUT);
  }

  auto& selector = vocbase.server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();

  // Step 1. Create new index object
  std::shared_ptr<Index> newIdx;
  try {
    newIdx = engine.indexFactory().prepareIndexFromSlice(info, /*generateKey*/ !restore,
                                                         _logicalCollection, false);
  } catch (std::exception const& ex) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_INDEX_CREATION_FAILED, ex.what());
  }

  // we cannot persist primary or edge indexes
  TRI_ASSERT(newIdx->type() != Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
  TRI_ASSERT(newIdx->type() != Index::IndexType::TRI_IDX_TYPE_EDGE_INDEX);

  {
    // Step 2. Check for existing matching index
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    if (auto existingIdx = findIndex(info, _indexes); existingIdx != nullptr) {
      // We already have this index.
      if (existingIdx->type() == arangodb::Index::TRI_IDX_TYPE_TTL_INDEX) {
        // special handling for TTL indexes
        // if there is exactly the same index present, we return it
        if (!existingIdx->matchesDefinition(info)) {
          // if there is another TTL index already, we make things abort here
          THROW_ARANGO_EXCEPTION_MESSAGE(
              TRI_ERROR_BAD_PARAMETER,
              "there can only be one ttl index per collection");
        }
      }
      // same index already exists. return it
      created = false;
      return existingIdx;
    }

    // check all existing indexes for id/new conflicts
    for (auto const& other : _indexes) {
      if (other->id() == newIdx->id() || other->name() == newIdx->name()) {
        // definition shares an identifier with an existing index with a
        // different definition
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
        VPackBuilder builder;
        other->toVelocyPack(builder, static_cast<std::underlying_type<Index::Serialize>::type>(
                                         Index::Serialize::Basics));
        LOG_TOPIC("29d1c", WARN, Logger::ENGINES)
            << "attempted to create index '" << info.toJson()
            << "' but found conflicting index '" << builder.slice().toJson() << "'";
#endif
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_ARANGO_DUPLICATE_IDENTIFIER,
                                       "duplicate value for `" +
                                           StaticStrings::IndexId + "` or `" +
                                           StaticStrings::IndexName + "`");
      }
    }
  }

  // until here we have been completely read only.
  // modifications start now...

  Result res = arangodb::basics::catchToResult([&]() -> Result {
    Result res;

    // Step 3. add index to collection entry (for removal after a crash)
    auto buildIdx = std::make_shared<RocksDBBuilderIndex>(
        std::static_pointer_cast<RocksDBIndex>(newIdx));
    if (!engine.inRecovery()) {
      // manually modify collection entry, other methods need lock
      RocksDBKey key;  // read collection info from database
      key.constructCollection(vocbase.id(), _logicalCollection.id());
      rocksdb::PinnableSlice ps;
      rocksdb::Status s =
          engine.db()->Get(rocksdb::ReadOptions(),
                           RocksDBColumnFamilyManager::get(
                               RocksDBColumnFamilyManager::Family::Definitions),
                           key.string(), &ps);
      if (!s.ok()) {
        return res.reset(rocksutils::convertStatus(s));
      }

      VPackBuilder builder;
      builder.openObject();
      for (auto pair : VPackObjectIterator(RocksDBValue::data(ps))) {
        if (pair.key.isEqualString("indexes")) {  // append new index
          VPackArrayBuilder arrGuard(&builder, "indexes");
          builder.add(VPackArrayIterator(pair.value));
          buildIdx->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Internals));
        } else {
          builder.add(pair.key);
          builder.add(pair.value);
        }
      }
      builder.close();
      res = engine.writeCreateCollectionMarker(vocbase.id(), _logicalCollection.id(),
                                               builder.slice(), RocksDBLogValue::Empty());
      if (res.fail()) {
        return res;
      }
    }

    // release inventory lock while we are filling the index
    inventoryLocker.unlock();

    // Step 4. fill index
    bool const inBackground =
        basics::VelocyPackHelper::getBooleanValue(info, StaticStrings::IndexInBackground, false);
    if (inBackground) {
      // allow concurrent inserts into index
      {
        RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
        _indexes.emplace(buildIdx);
      }

      res = buildIdx->fillIndexBackground(locker);
    } else {
      res = buildIdx->fillIndexForeground();
    }
    if (res.fail()) {
      return res;
    }

    // always (re-)lock to avoid inconsistencies
    locker.lock();

    inventoryLocker.lock();

    // Step 5. register in index list
    {
      RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
      if (inBackground) {
        // remove temporary index and swap in actual index
        removeIndex(_indexes, buildIdx->id());
      }
      _indexes.emplace(newIdx);
    }
#if USE_PLAN_CACHE
    arangodb::aql::PlanCache::instance()->invalidate(vocbase);
#endif

    // inBackground index might not recover selectivity estimate w/o sync
    if (inBackground && !newIdx->unique() && newIdx->hasSelectivityEstimate()) {
      engine.settingsManager()->sync(false);
    }

    // Step 6. persist in rocksdb
    if (!engine.inRecovery()) {
      // write new collection marker
      auto builder =
          _logicalCollection.toVelocyPackIgnore({"path", "statusString"},
                                                LogicalDataSource::Serialization::PersistenceWithInProgress);
      VPackBuilder indexInfo;
      newIdx->toVelocyPack(indexInfo, Index::makeFlags(Index::Serialize::Internals));
      res = engine.writeCreateCollectionMarker(
          vocbase.id(), _logicalCollection.id(), builder.slice(),
          RocksDBLogValue::IndexCreate(vocbase.id(), _logicalCollection.id(),
                                       indexInfo.slice()));
    }

    return res;
  });

  if (res.ok()) {
    created = true;
    return newIdx;
  }

  // cleanup routine
  // We could not create the index. Better abort
  {
    RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
    removeIndex(_indexes, newIdx->id());
  }
  newIdx->drop();
  THROW_ARANGO_EXCEPTION(res);
}

/// @brief Drop an index with the given iid.
bool RocksDBCollection::dropIndex(IndexId iid) {
  // usually always called when _exclusiveLock is held
  if (iid.empty() || iid.isPrimary()) {
    return true;
  }

  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  bool const inRecovery = engine.inRecovery();

  std::shared_ptr<arangodb::Index> toRemove;
  {
    RECURSIVE_WRITE_LOCKER(_indexesLock, _indexesLockWriteOwner);
    for (auto& it : _indexes) {
      if (iid == it->id()) {
        toRemove = it;
        _indexes.erase(it);
        break;
      }
    }

    if (!toRemove) {  // index not found
      // We tried to remove an index that does not exist
      events::DropIndex(_logicalCollection.vocbase().name(), _logicalCollection.name(),
                        std::to_string(iid.id()), TRI_ERROR_ARANGO_INDEX_NOT_FOUND);
      return false;
    }

    // skip writing WAL marker if inRecovery()
    if (!inRecovery) {
      auto builder =
          _logicalCollection.toVelocyPackIgnore({"path", "statusString"},
                                                LogicalDataSource::Serialization::PersistenceWithInProgress);
      // log this event in the WAL and in the collection meta-data
      Result res = engine.writeCreateCollectionMarker(  // write marker
          _logicalCollection.vocbase().id(),            // vocbase id
          _logicalCollection.id(),                      // collection id
          builder.slice(),                              // RocksDB path
          RocksDBLogValue::IndexDrop(                   // marker
              _logicalCollection.vocbase().id(), _logicalCollection.id(), iid  // args
              ));

      if (res.fail()) {
        return false;
      }
    }
  }

  TRI_ASSERT(toRemove != nullptr);

  RocksDBIndex* cindex = static_cast<RocksDBIndex*>(toRemove.get());
  TRI_ASSERT(cindex != nullptr);

  Result res = cindex->drop();

  if (res.ok()) {
    events::DropIndex(_logicalCollection.vocbase().name(), _logicalCollection.name(),
                      std::to_string(iid.id()), TRI_ERROR_NO_ERROR);

    cindex->compact();  // trigger compaction before deleting the object
  }

  return res.ok();
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAllIterator(transaction::Methods* trx,
                                                                 ReadOwnWrites readOwnWrites) const {
  return std::make_unique<RocksDBAllIndexIterator>(&_logicalCollection, trx, readOwnWrites);
}

std::unique_ptr<IndexIterator> RocksDBCollection::getAnyIterator(transaction::Methods* trx) const {
  return std::make_unique<RocksDBAnyIndexIterator>(&_logicalCollection, trx);
}

std::unique_ptr<ReplicationIterator> RocksDBCollection::getReplicationIterator(
    ReplicationIterator::Ordering order, uint64_t batchId) {
  if (order != ReplicationIterator::Ordering::Revision) {
    // not supported
    return nullptr;
  }

  EngineSelectorFeature& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  RocksDBEngine& engine = selector.engine<RocksDBEngine>();
  RocksDBReplicationManager* manager = engine.replicationManager();
  RocksDBReplicationContext* ctx = batchId == 0 ? nullptr : manager->find(batchId);
  auto guard = scopeGuard([manager, ctx]() noexcept {
    if (ctx) {
      manager->release(ctx);
    }
  });
  rocksdb::Snapshot const* snapshot = ctx ? ctx->snapshot() : nullptr;

  return std::make_unique<RocksDBRevisionReplicationIterator>(_logicalCollection, snapshot);
}

std::unique_ptr<ReplicationIterator> RocksDBCollection::getReplicationIterator(
    ReplicationIterator::Ordering order, transaction::Methods& trx) {
  if (order != ReplicationIterator::Ordering::Revision) {
    // not supported
    return nullptr;
  }

  return std::make_unique<RocksDBRevisionReplicationIterator>(_logicalCollection, trx);
}

////////////////////////////////////
// -- SECTION DML Operations --
///////////////////////////////////

Result RocksDBCollection::truncate(transaction::Methods& trx, OperationOptions& options) {
  TRI_ASSERT(!RocksDBTransactionState::toState(&trx)->isReadOnlyTransaction());

  ::TruncateTimeTracker timeTracker(_statistics._rocksdb_truncate_sec, _statistics, options);

  TRI_ASSERT(objectId() != 0);
  auto state = RocksDBTransactionState::toState(&trx);
  RocksDBTransactionMethods* mthds = state->rocksdbMethods();

  if (state->isOnlyExclusiveTransaction() &&
      state->hasHint(transaction::Hints::Hint::ALLOW_RANGE_DELETE) &&
      this->canUseRangeDeleteInWal() && _meta.numberDocuments() >= 32 * 1024) {
    // non-transactional truncate optimization. We perform a bunch of
    // range deletes and circumvent the normal rocksdb::Transaction.
    // no savepoint needed here
    TRI_ASSERT(!state->hasOperations());  // not allowed

    TRI_IF_FAILURE("RocksDBRemoveLargeRangeOn") {
      return Result(TRI_ERROR_DEBUG);
    }

    RocksDBEngine& engine = _logicalCollection.vocbase()
                                .server()
                                .getFeature<EngineSelectorFeature>()
                                .engine<RocksDBEngine>();
    rocksdb::DB* db = engine.db()->GetRootDB();

    TRI_IF_FAILURE("RocksDBCollection::truncate::forceSync") {
      engine.settingsManager()->sync(false);
    }

    // pre commit sequence needed to place a blocker
    RocksDBBlockerGuard blocker(&_logicalCollection);
    blocker.placeBlocker(state->id());

    rocksdb::WriteBatch batch;
    // delete documents
    RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
    rocksdb::Status s =
        batch.DeleteRange(bounds.columnFamily(), bounds.start(), bounds.end());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    // delete indexes, place estimator blockers
    {
      RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
      for (std::shared_ptr<Index> const& idx : _indexes) {
        RocksDBIndex* ridx = static_cast<RocksDBIndex*>(idx.get());
        bounds = ridx->getBounds();
        s = batch.DeleteRange(bounds.columnFamily(), bounds.start(), bounds.end());
        if (!s.ok()) {
          return rocksutils::convertStatus(s);
        }
      }
    }

    // add the log entry so we can recover the correct count
    auto log = RocksDBLogValue::CollectionTruncate(trx.vocbase().id(),
                                                   _logicalCollection.id(), objectId());

    s = batch.PutLogData(log.slice());

    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    rocksdb::WriteOptions wo;

    s = db->Write(wo, &batch);

    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }

    rocksdb::SequenceNumber seq = db->GetLatestSequenceNumber() - 1;  // post commit sequence

    uint64_t numDocs = _meta.numberDocuments();
    _meta.adjustNumberDocuments(seq, /*revision*/ newRevisionId(),
                                -static_cast<int64_t>(numDocs));

    {
      RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
      for (std::shared_ptr<Index> const& idx : _indexes) {
        idx->afterTruncate(seq, &trx);  // clears caches / clears links (if applicable)
      }
    }
    bufferTruncate(seq);

    TRI_ASSERT(!state->hasOperations());  // not allowed
    return {};
  }

  TRI_IF_FAILURE("RocksDBRemoveLargeRangeOff") { return {TRI_ERROR_DEBUG}; }

  // normal transactional truncate
  RocksDBKeyBounds documentBounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  rocksdb::Comparator const* cmp =
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents)
          ->GetComparator();
  rocksdb::Slice const end = documentBounds.end();

  // avoid OOM error for truncate by committing earlier
  uint64_t const prvICC = state->options().intermediateCommitCount;
  if (!state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    state->options().intermediateCommitCount = std::min<uint64_t>(prvICC, 10000);
  }

  // push our current transaction on the stack
  state->beginQuery(true);
  auto stateGuard = scopeGuard([state, prvICC]() noexcept {
    state->endQuery(true);
    // reset to previous value after truncate is finished
    state->options().intermediateCommitCount = prvICC;
  });

  uint64_t found = 0;

  VPackBuilder docBuffer;
  auto iter = mthds->NewIterator(documentBounds.columnFamily(), [&](ReadOptions& ro) {
    ro.iterate_upper_bound = &end;
    // we are going to blow away all data anyway. no need to blow up the cache
    ro.fill_cache = false;
    ro.readOwnWrites = false;
    TRI_ASSERT(ro.snapshot);
  });
  for (iter->Seek(documentBounds.start());
       iter->Valid() && cmp->Compare(iter->key(), end) < 0; iter->Next()) {
    ++found;
    TRI_ASSERT(objectId() == RocksDBKey::objectId(iter->key()));
    VPackSlice document(reinterpret_cast<uint8_t const*>(iter->value().data()));
    TRI_ASSERT(document.isObject());

    // tmp may contain a pointer into rocksdb::WriteBuffer::_rep. This is
    // a 'std::string' which might be realloc'ed on any Put/Delete operation
    docBuffer.clear();
    docBuffer.add(document);

    // To print the WAL we need key and RID
    VPackSlice key;
    RevisionId rid = RevisionId::none();
    transaction::helpers::extractKeyAndRevFromDocument(document, key, rid);
    TRI_ASSERT(key.isString());
    TRI_ASSERT(rid.isSet());

    RocksDBSavePoint savepoint(state, &trx, TRI_VOC_DOCUMENT_OPERATION_REMOVE);

    LocalDocumentId const docId = RocksDBKey::documentId(iter->key());
    auto res = removeDocument(&trx, savepoint, docId, docBuffer.slice(), options, rid);

    if (res.ok()) {
      res = savepoint.finish(_logicalCollection.id(), newRevisionId());
    }

    if (res.fail()) {  // Failed to remove document in truncate.
      return res;
    }
    trackWaitForSync(&trx, options);
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (state->numCommits() == 0) {
    // check IN TRANSACTION if documents have been deleted
    if (mthds->countInBounds(RocksDBKeyBounds::CollectionDocuments(objectId()), true)) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                     "deletion check in collection truncate "
                                     "failed - not all documents have been "
                                     "deleted");
    }
  }
#endif

  TRI_IF_FAILURE("FailAfterAllCommits") { return Result(TRI_ERROR_DEBUG); }
  TRI_IF_FAILURE("SegfaultAfterAllCommits") {
    TRI_TerminateDebugging("SegfaultAfterAllCommits");
  }
  return {};
}

Result RocksDBCollection::lookupKey(transaction::Methods* trx, VPackStringRef key,
                                    std::pair<LocalDocumentId, RevisionId>& result,
                                    ReadOwnWrites readOwnWrites) const {
  result.first = LocalDocumentId::none();
  result.second = RevisionId::none();

  // lookup the revision id in the primary index
  if (!primaryIndex()->lookupRevision(trx, key, result.first, result.second, readOwnWrites)) {
    // document not found
    TRI_ASSERT(!result.first.isSet());
    TRI_ASSERT(result.second.empty());
    return Result(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }

  // document found, but revisionId may not have been present in the primary
  // index. this can happen for "older" collections
  TRI_ASSERT(result.first.isSet());
  TRI_ASSERT(result.second.isSet());
  return {};
}

bool RocksDBCollection::lookupRevision(transaction::Methods* trx,
                                       VPackSlice const& key, RevisionId& revisionId,
                                       ReadOwnWrites readOwnWrites) const {
  TRI_ASSERT(key.isString());
  LocalDocumentId documentId;
  revisionId = RevisionId::none();
  // lookup the revision id in the primary index
  if (!primaryIndex()->lookupRevision(trx, arangodb::velocypack::StringRef(key),
                                      documentId, revisionId, readOwnWrites)) {
    // document not found
    TRI_ASSERT(revisionId.empty());
    return false;
  }

  // document found, and we have a valid revisionId
  TRI_ASSERT(documentId.isSet());
  TRI_ASSERT(revisionId.isSet());
  return true;
}

Result RocksDBCollection::read(transaction::Methods* trx,
                               arangodb::velocypack::StringRef const& key,
                               IndexIterator::DocumentCallback const& cb,
                               ReadOwnWrites readOwnWrites) const {
  TRI_IF_FAILURE("LogicalCollection::read") { return Result(TRI_ERROR_DEBUG); }

  ::ReadTimeTracker timeTracker(_statistics._rocksdb_read_sec, _statistics);

  rocksdb::PinnableSlice ps;
  Result res;
  LocalDocumentId documentId;
  do {
    documentId = primaryIndex()->lookupKey(trx, key, readOwnWrites);
    if (!documentId.isSet()) {
      res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      return res;
    }  // else found

    TRI_IF_FAILURE("RocksDBCollection::read-delay") {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(RandomGenerator::interval(uint32_t(2000))));
    }

    res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/ true,
                              /*fillCache*/ true, readOwnWrites);
    if (res.ok()) {
      cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));
    }
  } while (res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) &&
           RocksDBTransactionState::toState(trx)->ensureSnapshot());
  ::reportPrimaryIndexInconsistency(res, key, documentId, _logicalCollection);
  return res;
}

// read using a local document id
Result RocksDBCollection::read(transaction::Methods* trx, LocalDocumentId const& documentId,
                               IndexIterator::DocumentCallback const& cb,
                               ReadOwnWrites readOwnWrites) const {
  ::ReadTimeTracker timeTracker(_statistics._rocksdb_read_sec, _statistics);

  if (!documentId.isSet()) {
    return Result{TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD,
                  "invalid local document id"};
  }

  return lookupDocumentVPack(trx, documentId, cb, /*withCache*/ true, readOwnWrites);
}

// read using a local document id
bool RocksDBCollection::readDocument(transaction::Methods* trx,
                                     LocalDocumentId const& documentId,
                                     ManagedDocumentResult& result,
                                     ReadOwnWrites readOwnWrites) const {
  ::ReadTimeTracker timeTracker(_statistics._rocksdb_read_sec, _statistics);

  bool ret = false;

  if (documentId.isSet()) {
    std::string* buffer = result.setManaged();
    rocksdb::PinnableSlice ps(buffer);
    Result res = lookupDocumentVPack(trx, documentId, ps, /*readCache*/ true,
                                     /*fillCache*/ true, readOwnWrites);
    if (res.ok()) {
      if (ps.IsPinned()) {
        buffer->assign(ps.data(), ps.size());
      }  // else value is already assigned
      ret = true;
      ;
    }
  }

  return ret;
}

Result RocksDBCollection::insert(arangodb::transaction::Methods* trx,
                                 arangodb::velocypack::Slice const slice,
                                 arangodb::ManagedDocumentResult& resultMdr,
                                 OperationOptions& options) {
  RocksDBTransactionStateGuard transactionStateGuard(RocksDBTransactionState::toState(trx));

  TRI_ASSERT(!RocksDBTransactionState::toState(trx)->isReadOnlyTransaction());

  ::WriteTimeTracker timeTracker(_statistics._rocksdb_insert_sec, _statistics, options);

  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());
  transaction::BuilderLeaser builder(trx);
  RevisionId revisionId;
  Result res(newObjectForInsert(trx, slice, isEdgeCollection, *builder.get(),
                                options.isRestore, revisionId));
  if (res.fail()) {
    return res;
  }

  VPackSlice newSlice = builder->slice();

  if (options.validate && !options.isRestore &&
      options.isSynchronousReplicationFrom.empty()) {
    // only do schema validation when we are not restoring/replicating
    res = _logicalCollection.validate(newSlice, trx->transactionContextPtr()->getVPackOptions());

    if (res.fail()) {
      return res;
    }
  }

  auto r = transaction::Methods::validateSmartJoinAttribute(_logicalCollection, newSlice);

  if (r != TRI_ERROR_NO_ERROR) {
    res.reset(r);
    return res;
  }

  LocalDocumentId const documentId = ::generateDocumentId(_logicalCollection, revisionId);

  auto* state = RocksDBTransactionState::toState(trx);
  RocksDBSavePoint savepoint(state, trx, TRI_VOC_DOCUMENT_OPERATION_INSERT);

  res = insertDocument(trx, savepoint, documentId, newSlice, options, revisionId);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newSlice.begin());
      TRI_ASSERT(resultMdr.revisionId() == revisionId);
    } else if (!options.silent) {  //  need to pass revId manually
      transaction::BuilderLeaser keyBuilder(trx);
      keyBuilder->openObject(/*unindexed*/ true);
      keyBuilder->add(StaticStrings::KeyString,
                      transaction::helpers::extractKeyFromDocument(newSlice));
      keyBuilder->close();
      resultMdr.setManaged()->assign(reinterpret_cast<char const*>(keyBuilder->start()),
                                     keyBuilder->size());
      resultMdr.setRevisionId(revisionId);
    }

    res = savepoint.finish(_logicalCollection.id(), revisionId);
  }

  return res;
}

Result RocksDBCollection::update(transaction::Methods* trx, velocypack::Slice newSlice,
                                 ManagedDocumentResult& resultMdr, OperationOptions& options,
                                 ManagedDocumentResult& previousMdr) {
  ::WriteTimeTracker timeTracker(_statistics._rocksdb_update_sec, _statistics, options);
  return performUpdateOrReplace(trx, newSlice, resultMdr, options, previousMdr, true);
}

Result RocksDBCollection::replace(transaction::Methods* trx, velocypack::Slice newSlice,
                                  ManagedDocumentResult& resultMdr, OperationOptions& options,
                                  ManagedDocumentResult& previousMdr) {
  ::WriteTimeTracker timeTracker(_statistics._rocksdb_replace_sec, _statistics, options);
  return performUpdateOrReplace(trx, newSlice, resultMdr, options, previousMdr, false);
}

Result RocksDBCollection::performUpdateOrReplace(transaction::Methods* trx,
                                                 velocypack::Slice newSlice,
                                                 ManagedDocumentResult& resultMdr,
                                                 OperationOptions& options,
                                                 ManagedDocumentResult& previousMdr,
                                                 bool isUpdate) {
  RocksDBTransactionStateGuard transactionStateGuard(RocksDBTransactionState::toState(trx));

  TRI_ASSERT(!RocksDBTransactionState::toState(trx)->isReadOnlyTransaction());

  Result res;

  VPackSlice keySlice = newSlice.get(StaticStrings::KeyString);
  if (keySlice.isNone()) {
    return res.reset(TRI_ERROR_ARANGO_DOCUMENT_HANDLE_BAD);
  } else if (!keySlice.isString()) {
    return res.reset(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }
  auto keyStr = VPackStringRef(keySlice);
  if (keyStr.empty()) {
    return res.reset(TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD);
  }

  // modifications always need to observe all changes in order to validate uniqueness constraints
  auto const oldDocumentId = primaryIndex()->lookupKey(trx, keyStr, ReadOwnWrites::yes);
  if (!oldDocumentId.isSet()) {
    return res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  // modifications always need to observe all changes in order to validate uniqueness constraints
  res = lookupDocumentVPack(trx, oldDocumentId, previousPS,
                            /*readCache*/ true, /*fillCache*/ false, ReadOwnWrites::yes);
  if (res.fail()) {
    ::reportPrimaryIndexInconsistency(res, keyStr, oldDocumentId, _logicalCollection);
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId().isSet());

  if (!options.ignoreRevs) {  // Check old revision:
    RevisionId expectedRev = RevisionId::fromSlice(newSlice);
    if (!checkRevision(trx, expectedRev, previousMdr.revisionId())) {
      return res.reset(TRI_ERROR_ARANGO_CONFLICT,
                       "conflict, _rev values do not match");
    }
  }

  // merge old and new values
  RevisionId revisionId;
  bool const isEdgeCollection = (TRI_COL_TYPE_EDGE == _logicalCollection.type());

  transaction::BuilderLeaser builder(trx);
  if (isUpdate) {
    res = mergeObjectsForUpdate(trx, oldDoc, newSlice, isEdgeCollection,
                                options.mergeObjects, options.keepNull,
                                *builder.get(), options.isRestore, revisionId);
  } else {
    res = newObjectForReplace(trx, oldDoc, newSlice, isEdgeCollection,
                              *builder.get(), options.isRestore, revisionId);
  }
  if (res.fail()) {
    return res;
  }
  LocalDocumentId const newDocumentId = ::generateDocumentId(_logicalCollection, revisionId);

  if (_isDBServer) {
    // Need to check that no sharding keys have changed:
    if (arangodb::shardKeysChanged(_logicalCollection, oldDoc, builder->slice(), isUpdate)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SHARDING_ATTRIBUTES);
    }
    if (arangodb::smartJoinAttributeChanged(_logicalCollection, oldDoc,
                                            builder->slice(), isUpdate)) {
      return res.reset(TRI_ERROR_CLUSTER_MUST_NOT_CHANGE_SMART_JOIN_ATTRIBUTE);
    }
  }

  VPackSlice const newDoc(builder->slice());

  if (options.validate && options.isSynchronousReplicationFrom.empty()) {
    res = _logicalCollection.validate(newDoc, oldDoc,
                                      trx->transactionContextPtr()->getVPackOptions());
    if (res.fail()) {
      return res;
    }
  }

  auto* state = RocksDBTransactionState::toState(trx);
  auto opType = isUpdate ? TRI_VOC_DOCUMENT_OPERATION_UPDATE : TRI_VOC_DOCUMENT_OPERATION_REPLACE;
  RocksDBSavePoint savepoint(state, trx, opType);

  res = modifyDocument(trx, savepoint, oldDocumentId, oldDoc, newDocumentId,
                       newDoc, options, revisionId);

  if (res.ok()) {
    trackWaitForSync(trx, options);

    if (options.returnNew) {
      resultMdr.setManaged(newDoc.begin());
      TRI_ASSERT(!resultMdr.empty());
    } else {  //  need to pass revId manually
      resultMdr.setRevisionId(revisionId);
    }
    if (options.returnOld) {
      if (previousPS.IsPinned()) {  // value was not copied
        prevBuffer->assign(previousPS.data(), previousPS.size());
      }  // else value is already assigned
      TRI_ASSERT(!previousMdr.empty());
    } else {
      previousMdr.clearData();
    }

    res = savepoint.finish(_logicalCollection.id(), revisionId);
    if (res.fail()) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }

  return res;
}

Result RocksDBCollection::remove(transaction::Methods& trx, velocypack::Slice slice,
                                 ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  RocksDBTransactionStateGuard transactionStateGuard(RocksDBTransactionState::toState(&trx));

  TRI_ASSERT(!RocksDBTransactionState::toState(&trx)->isReadOnlyTransaction());

  ::WriteTimeTracker timeTracker(_statistics._rocksdb_remove_sec, _statistics, options);

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
  auto keyStr = VPackStringRef(keySlice);
  if (keyStr.empty()) {
    return TRI_ERROR_ARANGO_DOCUMENT_KEY_BAD;
  }

  // modifications always need to observe all changes in order to validate uniqueness constraints
  auto const documentId = primaryIndex()->lookupKey(&trx, keyStr, ReadOwnWrites::yes);
  if (!documentId.isSet()) {
    return TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND;
  }

  // Check old revision:
  RevisionId expectedId = RevisionId::none();
  if (!options.ignoreRevs && slice.isObject()) {
    expectedId = RevisionId::fromSlice(slice);
  }

  Result res = remove(trx, documentId, expectedId, previousMdr, options);
  ::reportPrimaryIndexInconsistency(res, keyStr, documentId, _logicalCollection);
  return res;
}

Result RocksDBCollection::remove(transaction::Methods& trx, LocalDocumentId documentId,
                                 ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  ::WriteTimeTracker timeTracker(_statistics._rocksdb_remove_sec, _statistics, options);

  return remove(trx, documentId, /*expectedRev*/ RevisionId::none(), previousMdr, options);
}

Result RocksDBCollection::remove(transaction::Methods& trx, LocalDocumentId documentId,
                                 RevisionId expectedRev, ManagedDocumentResult& previousMdr,
                                 OperationOptions& options) {
  // all callers that get here must make sure that this operation is properly
  // counted and time-tracked, metrics-wise!

  Result res;
  if (!documentId.isSet()) {
    return res.reset(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
  }
  std::string* prevBuffer = previousMdr.setManaged();
  // uses either prevBuffer or avoids memcpy (if read hits block cache)
  rocksdb::PinnableSlice previousPS(prevBuffer);
  // modifications always need to observe all changes in order to validate uniqueness constraints
  res = lookupDocumentVPack(&trx, documentId, previousPS,
                            /*readCache*/ true, /*fillCache*/ false, ReadOwnWrites::yes);
  if (res.fail()) {
    return res;
  }

  TRI_ASSERT(previousPS.size() > 0);
  VPackSlice const oldDoc(reinterpret_cast<uint8_t const*>(previousPS.data()));
  previousMdr.setRevisionId(transaction::helpers::extractRevFromDocument(oldDoc));
  TRI_ASSERT(previousMdr.revisionId().isSet());

  // Check old revision:
  if (!options.ignoreRevs && expectedRev.isSet()) {
    if (!checkRevision(&trx, expectedRev, previousMdr.revisionId())) {
      return res.reset(TRI_ERROR_ARANGO_CONFLICT,
                       "conflict, _rev values do not match");
    }
  }

  auto* state = RocksDBTransactionState::toState(&trx);
  RocksDBSavePoint savepoint(state, &trx, TRI_VOC_DOCUMENT_OPERATION_REMOVE);

  res = removeDocument(&trx, savepoint, documentId, oldDoc, options,
                       previousMdr.revisionId());

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

    res = savepoint.finish(_logicalCollection.id(), newRevisionId());
  }

  return res;
}

bool RocksDBCollection::hasDocuments() {
  RocksDBEngine& engine =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>().engine<RocksDBEngine>();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  return rocksutils::hasKeys(engine.db(), bounds, /*snapshot*/ nullptr, true);
}

/// @brief return engine-specific figures
void RocksDBCollection::figuresSpecific(bool details, arangodb::velocypack::Builder& builder) {
  auto& selector =
      _logicalCollection.vocbase().server().getFeature<EngineSelectorFeature>();
  auto& engine = selector.engine<RocksDBEngine>();
  rocksdb::TransactionDB* db = engine.db();
  RocksDBKeyBounds bounds = RocksDBKeyBounds::CollectionDocuments(objectId());
  rocksdb::Range r(bounds.start(), bounds.end());

  uint64_t out = 0;
  db->GetApproximateSizes(
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents),
      &r, 1, &out,
      static_cast<uint8_t>(rocksdb::DB::SizeApproximationFlags::INCLUDE_MEMTABLES |
                           rocksdb::DB::SizeApproximationFlags::INCLUDE_FILES));

  builder.add("documentsSize", VPackValue(out));
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

  if (details) {
    // engine-specific stuff here
    RocksDBFilePurgePreventer purgePreventer(engine.disallowPurging());

    rocksdb::DB* rootDB = db->GetRootDB();

    // acquire a snapshot
    rocksdb::Snapshot const* snapshot = db->GetSnapshot();

    try {
      builder.add("engine", VPackValue(VPackValueType::Object));

      builder.add("documents", VPackValue(rocksutils::countKeyRange(
                                   rootDB, RocksDBKeyBounds::CollectionDocuments(objectId()),
                                   snapshot, true)));
      builder.add("indexes", VPackValue(VPackValueType::Array));

      RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);
      for (auto it : _indexes) {
        auto type = it->type();
        if (type == Index::TRI_IDX_TYPE_UNKNOWN || type == Index::TRI_IDX_TYPE_IRESEARCH_LINK ||
            type == Index::TRI_IDX_TYPE_NO_ACCESS_INDEX) {
          continue;
        }

        builder.openObject();
        builder.add("type", VPackValue(it->typeName()));
        builder.add("id", VPackValue(it->id().id()));

        RocksDBIndex const* rix = static_cast<RocksDBIndex const*>(it.get());
        size_t count = 0;
        switch (type) {
          case Index::TRI_IDX_TYPE_PRIMARY_INDEX:
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::PrimaryIndex(rix->objectId()),
                                              snapshot, true);
            break;
          case Index::TRI_IDX_TYPE_GEO_INDEX:
          case Index::TRI_IDX_TYPE_GEO1_INDEX:
          case Index::TRI_IDX_TYPE_GEO2_INDEX:
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::GeoIndex(rix->objectId()),
                                              snapshot, true);
            break;
          case Index::TRI_IDX_TYPE_HASH_INDEX:
          case Index::TRI_IDX_TYPE_SKIPLIST_INDEX:
          case Index::TRI_IDX_TYPE_TTL_INDEX:
          case Index::TRI_IDX_TYPE_PERSISTENT_INDEX:
            if (it->unique()) {
              count = rocksutils::countKeyRange(
                  db, RocksDBKeyBounds::UniqueVPackIndex(rix->objectId(), false),
                  snapshot, true);
            } else {
              count = rocksutils::countKeyRange(
                  db, RocksDBKeyBounds::VPackIndex(rix->objectId(), false), snapshot, true);
            }
            break;
          case Index::TRI_IDX_TYPE_EDGE_INDEX:
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::EdgeIndex(rix->objectId()),
                                              snapshot, false);
            break;
          case Index::TRI_IDX_TYPE_FULLTEXT_INDEX:
            count = rocksutils::countKeyRange(db, RocksDBKeyBounds::FulltextIndex(rix->objectId()),
                                              snapshot, true);
            break;
          default:
            // we should not get here
            TRI_ASSERT(false);
        }

        builder.add("count", VPackValue(count));
        builder.close();
      }

      db->ReleaseSnapshot(snapshot);
    } catch (...) {
      // always free the snapshot
      db->ReleaseSnapshot(snapshot);
      throw;
    }
    builder.close();  // "indexes" array
    builder.close();  // "engine" object
  }
}

Result RocksDBCollection::insertDocument(arangodb::transaction::Methods* trx,
                                         RocksDBSavePoint& savepoint,
                                         LocalDocumentId const& documentId,
                                         VPackSlice doc, OperationOptions const& options,
                                         RevisionId const& revisionId) const {
  savepoint.prepareOperation(_logicalCollection.id(), revisionId);

  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  Result res;

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  RocksDBMethods* mthds = state->rocksdbMethods();

  TRI_ASSERT(!options.checkUniqueConstraintsInPreflight ||
             state->isOnlyExclusiveTransaction());

  bool const performPreflightChecks = (options.checkUniqueConstraintsInPreflight ||
                                       state->numOperations() >= ::preflightThreshold);

  if (performPreflightChecks) {
    // do a round of checks for all indexes, to verify that the insertion
    // will work (i.e. that there will be no unique constraint violations
    // later - we can't guard against disk full etc. later).
    // if this check already fails, there is no need to carry out the
    // actual index insertion, which will fail anyway, and in addition
    // spoil the current WriteBatch, which on a RollbackToSavePoint will
    // need to be completely reconstructed. the reconstruction of write
    // batches is super expensive, so we try to avoid it here.
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    for (auto const& idx : _indexes) {
      RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(idx.get());
      res = rIdx->checkInsert(*trx, mthds, documentId, doc, options);
      if (res.fail()) {
        return res;
      }
    }
  }

  TRI_ASSERT(res.ok());

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);
  TRI_ASSERT(key->containsLocalDocumentId(documentId));

  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // banish new document to avoid caching without committing first
    invalidateCacheEntry(key.ref());
  }

  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, state->isSingleOperation());

  TRI_IF_FAILURE("RocksDBCollection::insertFail1") {
    if (RandomGenerator::interval(uint32_t(1000)) >= 995) {
      return res.reset(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("RocksDBCollection::insertFail1Always") {
    return res.reset(TRI_ERROR_DEBUG);
  }

  rocksdb::Status s = mthds->PutUntracked(
      RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents),
      key.ref(),
      rocksdb::Slice(doc.startAs<char>(), static_cast<size_t>(doc.byteSize())));
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::document));
    res.withError([&doc](result::Error& err) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      err.appendErrorMessage("; key: ");
      err.appendErrorMessage(doc.get(StaticStrings::KeyString).copyString());
    });
    return res;
  }

  // we have successfully added a value to the WBWI. after this, we
  // can only restore the previous state via a full rebuild
  savepoint.tainted();

  {
    bool needReversal = false;
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    for (auto it = _indexes.begin(); it != _indexes.end(); ++it) {
      TRI_IF_FAILURE("RocksDBCollection::insertFail2") {
        if (it == _indexes.begin() && RandomGenerator::interval(uint32_t(1000)) >= 995) {
          return res.reset(TRI_ERROR_DEBUG);
        }
      }

      TRI_IF_FAILURE("RocksDBCollection::insertFail2Always") {
        return res.reset(TRI_ERROR_DEBUG);
      }

      RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(it->get());
      // if we already performed the preflight checks, there is no need to
      // repeat the checks once again here
      res = rIdx->insert(*trx, mthds, documentId, doc, options,
                         /*performChecks*/ !performPreflightChecks);
      // currently only IResearchLink indexes need a reversal
      needReversal = needReversal || rIdx->needsReversal();

      if (res.fail()) {
        if (needReversal && !state->isSingleOperation()) {
          ::reverseIdxOps(_indexes, it, [mthds, trx, &documentId, &doc](RocksDBIndex* rid) {
            return rid->remove(*trx, mthds, documentId, doc);
          });
        }
        break;
      }
    }
  }

  if (res.ok()) {
    TRI_ASSERT(revisionId == RevisionId::fromSlice(doc));
    state->trackInsert(_logicalCollection.id(), revisionId);
  }

  return res;
}

Result RocksDBCollection::removeDocument(arangodb::transaction::Methods* trx,
                                         RocksDBSavePoint& savepoint,
                                         LocalDocumentId const& documentId,
                                         VPackSlice doc, OperationOptions const& options,
                                         RevisionId const& revisionId) const {
  savepoint.prepareOperation(_logicalCollection.id(), revisionId);

  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);
  TRI_ASSERT(key->containsLocalDocumentId(documentId));

  invalidateCacheEntry(key.ref());

  RocksDBMethods* mthds = RocksDBTransactionState::toMethods(trx);

  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  TRI_IF_FAILURE("RocksDBCollection::removeFail1") {
    if (RandomGenerator::interval(uint32_t(1000)) >= 995) {
      return res.reset(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("RocksDBCollection::removeFail1Always") {
    return res.reset(TRI_ERROR_DEBUG);
  }

  rocksdb::Status s =
      mthds->SingleDelete(RocksDBColumnFamilyManager::get(
                              RocksDBColumnFamilyManager::Family::Documents),
                          key.ref());
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::document));
    res.withError([&doc](result::Error& err) {
      TRI_ASSERT(doc.get(StaticStrings::KeyString).isString());
      err.appendErrorMessage("; key: ");
      err.appendErrorMessage(doc.get(StaticStrings::KeyString).copyString());
    });
    return res;
  }

  // we have successfully removed a value from the WBWI. after this, we
  // can only restore the previous state via a full rebuild
  savepoint.tainted();

  {
    bool needReversal = false;
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    for (auto it = _indexes.begin(); it != _indexes.end(); it++) {
      TRI_IF_FAILURE("RocksDBCollection::removeFail2") {
        if (it == _indexes.begin() && RandomGenerator::interval(uint32_t(1000)) >= 995) {
          return res.reset(TRI_ERROR_DEBUG);
        }
      }
      TRI_IF_FAILURE("RocksDBCollection::removeFail2Always") {
        return res.reset(TRI_ERROR_DEBUG);
      }

      RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(it->get());
      res = rIdx->remove(*trx, mthds, documentId, doc);
      needReversal = needReversal || rIdx->needsReversal();
      if (res.fail()) {
        if (needReversal && !trx->isSingleOperationTransaction()) {
          ::reverseIdxOps(_indexes, it, [mthds, trx, &documentId, &doc](RocksDBIndex* rid) {
            OperationOptions options;
            options.indexOperationMode = IndexOperationMode::rollback;
            return rid->insert(*trx, mthds, documentId, doc, options, /*performChecks*/ true);
          });
        }
        break;
      }
    }
  }

  if (res.ok()) {
    RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
    TRI_ASSERT(revisionId == RevisionId::fromSlice(doc));
    state->trackRemove(_logicalCollection.id(), revisionId);
  }

  return res;
}

Result RocksDBCollection::modifyDocument(transaction::Methods* trx, RocksDBSavePoint& savepoint,
                                         LocalDocumentId const& oldDocumentId,
                                         VPackSlice const& oldDoc,
                                         LocalDocumentId const& newDocumentId,
                                         VPackSlice const& newDoc, OperationOptions& options,
                                         RevisionId const& revisionId) const {
  savepoint.prepareOperation(_logicalCollection.id(), revisionId);

  // Coordinator doesn't know index internals
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBTransactionState* state = RocksDBTransactionState::toState(trx);
  RocksDBMethods* mthds = state->rocksdbMethods();

  TRI_ASSERT(!options.checkUniqueConstraintsInPreflight ||
             state->isOnlyExclusiveTransaction());

  bool const performPreflightChecks = (options.checkUniqueConstraintsInPreflight ||
                                       state->numOperations() >= ::preflightThreshold);

  if (performPreflightChecks) {
    // do a round of checks for all indexes, to verify that the insertion
    // will work (i.e. that there will be no unique constraint violations
    // later - we can't guard against disk full etc. later).
    // if this check already fails, there is no need to carry out the
    // actual index insertion, which will fail anyway, and in addition
    // spoil the current WriteBatch, which on a RollbackToSavePoint will
    // need to be completely reconstructed. the reconstruction of write
    // batches is super expensive, so we try to avoid it here.
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    for (auto const& idx : _indexes) {
      RocksDBIndex* rIdx = static_cast<RocksDBIndex*>(idx.get());
      res = rIdx->checkReplace(*trx, mthds, oldDocumentId, newDoc, options);
      if (res.fail()) {
        return res;
      }
    }
  }

  // disable indexing in this transaction if we are allowed to
  IndexingDisabler disabler(mthds, trx->isSingleOperationTransaction());

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), oldDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(oldDocumentId));
  invalidateCacheEntry(key.ref());

  TRI_IF_FAILURE("RocksDBCollection::modifyFail1") {
    if (RandomGenerator::interval(uint32_t(1000)) >= 995) {
      return res.reset(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("RocksDBCollection::modifyFail1Always") {
    return res.reset(TRI_ERROR_DEBUG);
  }

  rocksdb::Status s =
      mthds->SingleDelete(RocksDBColumnFamilyManager::get(
                              RocksDBColumnFamilyManager::Family::Documents),
                          key.ref());
  if (!s.ok()) {
    res.reset(rocksutils::convertStatus(s, rocksutils::document));
    res.withError([&newDoc](result::Error& err) {
      TRI_ASSERT(newDoc.get(StaticStrings::KeyString).isString());
      err.appendErrorMessage("; key: ");
      err.appendErrorMessage(newDoc.get(StaticStrings::KeyString).copyString());
    });
    return res;
  }

  // we have successfully removed a value from the WBWI. after this, we
  // can only restore the previous state via a full rebuild
  savepoint.tainted();

  TRI_IF_FAILURE("RocksDBCollection::modifyFail2") {
    if (RandomGenerator::interval(uint32_t(1000)) >= 995) {
      return res.reset(TRI_ERROR_DEBUG);
    }
  }

  TRI_IF_FAILURE("RocksDBCollection::modifyFail2Always") {
    return res.reset(TRI_ERROR_DEBUG);
  }

  key->constructDocument(objectId(), newDocumentId);
  TRI_ASSERT(key->containsLocalDocumentId(newDocumentId));
  s = mthds->PutUntracked(RocksDBColumnFamilyManager::get(
                              RocksDBColumnFamilyManager::Family::Documents),
                          key.ref(),
                          rocksdb::Slice(newDoc.startAs<char>(),
                                         static_cast<size_t>(newDoc.byteSize())));
  if (!s.ok()) {
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  if (state->hasHint(transaction::Hints::Hint::GLOBAL_MANAGED)) {
    // banish new document to avoid caching without committing first
    invalidateCacheEntry(key.ref());
  }

  {
    bool needReversal = false;
    RECURSIVE_READ_LOCKER(_indexesLock, _indexesLockWriteOwner);

    for (auto it = _indexes.begin(); it != _indexes.end(); it++) {
      TRI_IF_FAILURE("RocksDBCollection::modifyFail3") {
        if (it == _indexes.begin() && RandomGenerator::interval(uint32_t(1000)) >= 995) {
          return res.reset(TRI_ERROR_DEBUG);
        }
      }

      TRI_IF_FAILURE("RocksDBCollection::modifyFail3Always") {
        return res.reset(TRI_ERROR_DEBUG);
      }

      auto rIdx = static_cast<RocksDBIndex*>(it->get());
      // if we already performed the preflight checks, there is no need to
      // repeat the checks once again here
      res = rIdx->update(*trx, mthds, oldDocumentId, oldDoc, newDocumentId, newDoc,
                         options, /*performChecks*/ !performPreflightChecks);
      needReversal = needReversal || rIdx->needsReversal();
      if (!res.ok()) {
        if (needReversal && !trx->isSingleOperationTransaction()) {
          ::reverseIdxOps(_indexes, it, [&](RocksDBIndex* rid) {
            return rid->update(*trx, mthds, newDocumentId, newDoc, oldDocumentId,
                               oldDoc, options, /*performChecks*/ true);
          });
        }
        break;
      }
    }
  }

  if (res.ok()) {
    TRI_ASSERT(revisionId == RevisionId::fromSlice(newDoc));
    state->trackRemove(_logicalCollection.id(), RevisionId::fromSlice(oldDoc));
    state->trackInsert(_logicalCollection.id(), revisionId);
  }

  return res;
}

/// @brief lookup document in cache and / or rocksdb
/// @param readCache attempt to read from cache
/// @param fillCache fill cache with found document
arangodb::Result RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                                        LocalDocumentId const& documentId,
                                                        rocksdb::PinnableSlice& ps,
                                                        bool readCache, bool fillCache,
                                                        ReadOwnWrites readOwnWrites) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);
  Result res;

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

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
      return res;  // all good
    }
    if (f.result().errorNumber() == TRI_ERROR_LOCK_TIMEOUT) {
      // assuming someone is currently holding a write lock, which
      // is why we cannot access the TransactionalBucket.
      lockTimeout = true;  // we skip the insert in this case
    }
  }

  RocksDBMethods* mthd = RocksDBTransactionState::toMethods(trx);
  rocksdb::Status s =
      mthd->Get(RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents),
                key->string(), &ps, readOwnWrites);

  if (!s.ok()) {
    LOG_TOPIC("f63dd", DEBUG, Logger::ENGINES)
        << "NOT FOUND rev: " << documentId.id()
        << " trx: " << trx->state()->id().id() << " objectID " << objectId()
        << " name: " << _logicalCollection.name();
    return res.reset(rocksutils::convertStatus(s, rocksutils::document));
  }

  if (fillCache && useCache() && !lockTimeout) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    size_t attempts = 0;
    cache::Cache::Inserter inserter(*_cache, key->string().data(),
                                    static_cast<uint32_t>(key->string().size()),
                                    ps.data(), static_cast<uint64_t>(ps.size()),
                                    [&attempts](Result const& res) -> bool {
                                      return res.is(TRI_ERROR_LOCK_TIMEOUT) &&
                                             ++attempts < 2;
                                    });
  }

  return res;
}

Result RocksDBCollection::lookupDocumentVPack(transaction::Methods* trx,
                                              LocalDocumentId const& documentId,
                                              IndexIterator::DocumentCallback const& cb,
                                              bool withCache,
                                              ReadOwnWrites readOwnWrites) const {
  TRI_ASSERT(trx->state()->isRunning());
  TRI_ASSERT(objectId() != 0);

  RocksDBKeyLeaser key(trx);
  key->constructDocument(objectId(), documentId);

  if (withCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // check cache first for fast path
    auto f = _cache->find(key->string().data(),
                          static_cast<uint32_t>(key->string().size()));
    if (f.found()) {
      cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(f.value()->value())));
      return Result{};
    }
  }

  transaction::StringLeaser buffer(trx);
  rocksdb::PinnableSlice ps(buffer.get());

  RocksDBMethods* mthd = RocksDBTransactionState::toMethods(trx);
  rocksdb::Status s =
      mthd->Get(RocksDBColumnFamilyManager::get(RocksDBColumnFamilyManager::Family::Documents),
                key->string(), &ps, readOwnWrites);

  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  TRI_ASSERT(ps.size() > 0);
  cb(documentId, VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));

  if (withCache && useCache()) {
    TRI_ASSERT(_cache != nullptr);
    // write entry back to cache
    size_t attempts = 0;
    cache::Cache::Inserter inserter(*_cache, key->string().data(),
                                    static_cast<uint32_t>(key->string().size()),
                                    ps.data(), static_cast<uint64_t>(ps.size()),
                                    [&attempts](Result const& res) -> bool {
                                      return res.is(TRI_ERROR_LOCK_TIMEOUT) &&
                                             ++attempts < 2;
                                    });
  }

  return Result{};
}

void RocksDBCollection::createCache() const {
  if (!_cacheEnabled || _cache || _logicalCollection.isAStub() ||
      ServerState::instance()->isCoordinator()) {
    // we leave this if we do not need the cache
    // or if cache already created
    return;
  }

  TRI_ASSERT(_cacheEnabled);
  TRI_ASSERT(_cache.get() == nullptr);
  auto* manager =
      _logicalCollection.vocbase().server().getFeature<CacheManagerFeature>().manager();
  TRI_ASSERT(manager != nullptr);
  LOG_TOPIC("f5df2", DEBUG, Logger::CACHE) << "Creating document cache";
  _cache = manager->createCache(cache::CacheType::Transactional);
  TRI_ASSERT(_cacheEnabled);
}

void RocksDBCollection::destroyCache() const {
  if (!_cache) {
    return;
  }
  auto* manager =
      _logicalCollection.vocbase().server().getFeature<CacheManagerFeature>().manager();
  TRI_ASSERT(manager != nullptr);
  // must have a cache...
  TRI_ASSERT(_cache.get() != nullptr);
  LOG_TOPIC("7137b", DEBUG, Logger::CACHE) << "Destroying document cache";
  manager->destroyCache(_cache);
  _cache.reset();
}

// banish given key from transactional cache
void RocksDBCollection::invalidateCacheEntry(RocksDBKey const& k) const {
  if (useCache()) {
    TRI_ASSERT(_cache != nullptr);
    bool banished = false;
    while (!banished) {
      auto status = _cache->banish(k.buffer()->data(),
                                   static_cast<uint32_t>(k.buffer()->size()));
      if (status.ok()) {
        banished = true;
      } else if (status.errorNumber() == TRI_ERROR_SHUTTING_DOWN) {
        destroyCache();
        break;
      }
    }
  }
}

/// @brief can use non transactional range delete in write ahead log
bool RocksDBCollection::canUseRangeDeleteInWal() const {
  if (ServerState::instance()->isSingleServer()) {
    // disableWalFilePruning is used by createIndex
    return _numIndexCreations.load(std::memory_order_acquire) == 0;
  }
  return false;
}

}  // namespace arangodb
