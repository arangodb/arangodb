////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationContext.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/MutexLocker.h"
#include "Basics/ScopeGuard.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringUtils.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Basics/system-functions.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Replication/ReplicationClients.h"
#include "Replication/ReplicationFeature.h"
#include "Replication/Syncer.h"
#include "Replication/common-defines.h"
#include "Replication/utilities.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMetaCollection.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBSettingsManager.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "Utilities/NameValidator.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

namespace {

DataSourceId normalizeIdentifier(TRI_vocbase_t& vocbase,
                                 std::string const& identifier) {
  DataSourceId id = DataSourceId::none();
  std::shared_ptr<LogicalCollection> logical{
      vocbase.lookupCollection(identifier)};

  if (logical) {
    id = logical->id();
  }

  return id;
}

rocksdb::SequenceNumber forceWrite(RocksDBEngine& engine) {
  auto* sm = engine.settingsManager();
  if (sm) {
    sm->sync(/*force*/ true);
  }
  return engine.db()->GetLatestSequenceNumber();
}

}  // namespace

template<typename T>
bool RocksDBReplicationContext::findCollection(
    std::string const& dbName, T const& collection,
    std::function<void(TRI_vocbase_t& vocbase,
                       LogicalCollection& collection)> const& cb) {
  auto& dbfeature = _engine.server().getFeature<DatabaseFeature>();
  auto vocbase = dbfeature.useDatabase(dbName);
  if (!vocbase) {
    return false;
  }

  std::shared_ptr<LogicalCollection> coll;
  try {
    coll = vocbase->useCollection(collection, /*checkPermissions*/ false);
  } catch (...) {
    // will fail if collection does not exist
  }
  if (!coll) {
    return false;
  }
  auto collectionGuard =
      scopeGuard([&]() noexcept { vocbase->releaseCollection(coll.get()); });

  cb(*vocbase, *coll);
  return true;
}

RocksDBReplicationContext::RocksDBReplicationContext(RocksDBEngine& engine,
                                                     double ttl,
                                                     SyncerId syncerId,
                                                     ServerId clientId)
    : _engine(engine),
      _id{TRI_NewTickServer()},
      _syncerId{syncerId},
      // buggy clients may not send the serverId
      _clientId{clientId.isSet() ? clientId : ServerId(_id)},
      _snapshotTick{0},
      _ttl{ttl > 0.0 ? ttl : replutils::BatchInfo::DefaultTimeout},
      _expires{TRI_microtime() + _ttl} {
  TRI_ASSERT(_ttl > 0.0);
  TRI_ASSERT(_patchCount.empty());
}

RocksDBReplicationContext::~RocksDBReplicationContext() {
  MUTEX_LOCKER(guard, _contextLock);
  _iterators.clear();

  size_t removed = 0;
  for (auto& it : _blockers) {
    for (auto& it2 : it.second) {
      try {
        findCollection(
            it.first, it2.first,
            [&](TRI_vocbase_t& vocbase, LogicalCollection& collection) {
              auto* rcoll =
                  static_cast<RocksDBMetaCollection*>(collection.getPhysical());
              rcoll->removeRevisionTreeBlocker(TransactionId(it2.second));
              ++removed;
            });
      } catch (std::exception const& ex) {
        LOG_TOPIC("7dca6", WARN, Logger::ENGINES)
            << "unable to remove blocker for revision tree in " << it.first
            << "/" << it2.first.id() << ": " << ex.what();
      }
    }
  }
}

TRI_voc_tick_t RocksDBReplicationContext::id() const { return _id; }

std::shared_ptr<rocksdb::ManagedSnapshot> RocksDBReplicationContext::snapshot()
    const {
  return _snapshot;
}

uint64_t RocksDBReplicationContext::snapshotTick() {
  MUTEX_LOCKER(locker, _contextLock);
  lazyCreateSnapshot();
  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);
  return _snapshot->snapshot()->GetSequenceNumber();
}

/// invalidate all iterators with that vocbase
bool RocksDBReplicationContext::containsVocbase(TRI_vocbase_t& vocbase) {
  bool found = false;

  MUTEX_LOCKER(locker, _contextLock);

  auto it = _iterators.begin();
  while (it != _iterators.end()) {
    if (it->second->vocbase.id() == vocbase.id()) {
      if (it->second->isUsed()) {
        LOG_TOPIC("543d4", WARN, Logger::REPLICATION)
            << "trying to delete used context";
        it++;
      } else {
        found = true;
        it = _iterators.erase(it);
      }
    } else {
      it++;
    }
  }

  return found;
}

/// invalidate all iterators with that collection
bool RocksDBReplicationContext::containsCollection(
    LogicalCollection& collection) const {
  MUTEX_LOCKER(locker, _contextLock);

  for (auto const& it : _iterators) {
    if (it.second->logical != nullptr &&
        it.second->logical->id() == collection.id()) {
      return true;
    }
  }
  return false;
}

/// remove matching iterator
void RocksDBReplicationContext::releaseIterators(TRI_vocbase_t& vocbase,
                                                 DataSourceId cid) {
  MUTEX_LOCKER(locker, _contextLock);

  auto it = _iterators.find(cid);

  if (it != _iterators.end()) {
    if (it->second->isUsed()) {
      LOG_TOPIC("74164", ERR, Logger::REPLICATION)
          << "trying to delete used iterator";
    } else {
      _iterators.erase(it);
    }
  } else {
    LOG_TOPIC("5a0a1", ERR, Logger::REPLICATION)
        << "trying to delete non-existent iterator";
  }
}

/// Bind collection for incremental sync
std::tuple<Result, DataSourceId, uint64_t>
RocksDBReplicationContext::bindCollectionIncremental(TRI_vocbase_t& vocbase,
                                                     std::string const& cname) {
  std::shared_ptr<LogicalCollection> logical{vocbase.lookupCollection(cname)};

  if (!logical) {
    return std::make_tuple(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
                           DataSourceId::none(), 0);
  }
  DataSourceId cid = logical->id();

  LOG_TOPIC("71235", TRACE, Logger::REPLICATION)
      << "binding replication context " << id() << " to collection '" << cname
      << "'";

  MUTEX_LOCKER(writeLocker, _contextLock);

  auto it = _iterators.find(cid);
  if (it != _iterators.end()) {  // nothing to do here
    return std::make_tuple(Result{}, it->second->logical->id(),
                           it->second->numberDocuments);
  }

  uint64_t numberDocuments;
  uint64_t documentCountAdjustmentTicket = 0;
  auto* rcoll = static_cast<RocksDBMetaCollection*>(logical->getPhysical());
  if (_snapshot == nullptr) {
    // only DBServers require a corrected document count
    const double to = ServerState::instance()->isDBServer() ? 10.0 : 1.0;
    auto lockGuard = scopeGuard([rcoll]() noexcept { rcoll->unlockWrite(); });
    if (!_patchCount.empty() && _patchCount == cname &&
        rcoll->lockWrite(to) == TRI_ERROR_NO_ERROR) {
      // fetch number docs and snapshot under exclusive lock
      // this should enable us to correct the count later
      documentCountAdjustmentTicket =
          rcoll->meta().documentCountAdjustmentTicket();
    } else {
      lockGuard.cancel();
    }
    numberDocuments = rcoll->meta().numberDocuments();
    lazyCreateSnapshot();
  } else {  // fetch non-exclusive
    numberDocuments = rcoll->meta().numberDocuments();
  }
  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);
  TRI_ASSERT(documentCountAdjustmentTicket == 0 ||
             (!_patchCount.empty() && _patchCount == cname));

  auto iter =
      std::make_unique<CollectionIterator>(vocbase, logical, true, _snapshot);
  auto result = _iterators.try_emplace(cid, std::move(iter));
  TRI_ASSERT(result.second);

  CollectionIterator* cIter = result.first->second.get();
  if (nullptr == cIter->iter) {
    _iterators.erase(cid);
    return std::make_tuple(
        Result(TRI_ERROR_INTERNAL, "could not create db iterators"),
        DataSourceId::none(), 0);
  }
  cIter->numberDocuments = numberDocuments;
  cIter->numberDocumentsDumped = 0;
  cIter->documentCountAdjustmentTicket = documentCountAdjustmentTicket;

  // Prefetch the revision tree of the collection:
  _collectionGuidOfPrefetchedRevisionTree.clear();
  _prefetchedRevisionTree =
      rcoll->revisionTree(_snapshot->snapshot()->GetSequenceNumber());
  if (_prefetchedRevisionTree != nullptr) {
    LOG_TOPIC("123aa", DEBUG, Logger::ENGINES)
        << "Have successfully prefetched revision tree for collection "
        << logical->guid();
    _collectionGuidOfPrefetchedRevisionTree = logical->guid();
  } else {
    LOG_TOPIC("123ab", INFO, Logger::ENGINES)
        << "Could not prefetch revision tree for collection "
        << logical->guid();
  }

  // we should have a valid iterator if there are documents in here
  TRI_ASSERT(numberDocuments == 0 || cIter->hasMore());
  return std::make_tuple(Result{}, cid, numberDocuments);
}

void RocksDBReplicationContext::removeBlocker(
    std::string const& dbName, std::string const& collectionName) {
  findCollection(
      dbName, collectionName,
      [&](TRI_vocbase_t& vocbase, LogicalCollection& collection) {
        DataSourceId id = collection.id();

        MUTEX_LOCKER(guard, _contextLock);

        auto it = _blockers.find(dbName);
        if (it != _blockers.end()) {
          auto it2 = (*it).second.find(id);

          if (it2 != (*it).second.end()) {
            auto* rcoll =
                static_cast<RocksDBMetaCollection*>(collection.getPhysical());
            rcoll->removeRevisionTreeBlocker(TransactionId((*it2).second));
            (*it).second.erase(it2);
          }
        }
      });
}

// returns inventory
Result RocksDBReplicationContext::getInventory(TRI_vocbase_t& vocbase,
                                               bool includeSystem,
                                               bool includeFoxxQueues,
                                               bool global,
                                               VPackBuilder& result) {
  auto nameFilter = [&](LogicalCollection const* collection) {
    std::string const& cname = collection->name();
    if (!includeSystem && NameValidator::isSystemName(cname)) {
      // exclude all system collections
      return false;
    }

    if (TRI_ExcludeCollectionReplication(cname, includeSystem,
                                         includeFoxxQueues)) {
      // collection is excluded from replication
      return false;
    }

    // all other cases should be included
    return true;
  };

  TRI_voc_tick_t tick = TRI_NewTickServer();
  VPackBuilder inventory;

  if (global) {
    // global inventory
    vocbase.server().getFeature<DatabaseFeature>().inventory(inventory, tick,
                                                             nameFilter);
  } else {
    // database-specific inventory
    inventory.openObject();
    vocbase.inventory(inventory, tick, nameFilter);
    inventory.close();
  }

  size_t created = 0;
  auto handleCollections = [&](std::string const& dbName,
                               VPackSlice collections) {
    TRI_ASSERT(collections.isArray());
    for (auto it2 : VPackArrayIterator(collections)) {
      DataSourceId collection(basics::StringUtils::uint64(
          it2.get({"parameters", "id"}).copyString()));

      findCollection(
          dbName, collection,
          [&](TRI_vocbase_t& vocbase, LogicalCollection& collection) {
            auto* rcoll =
                static_cast<RocksDBMetaCollection*>(collection.getPhysical());
            TransactionId trxId{0};
            auto blockerGuard =
                scopeGuard([&]() noexcept {  // remove blocker afterwards
                  if (trxId.isSet()) {
                    rcoll->removeRevisionTreeBlocker(trxId);
                  }
                });
            trxId = TransactionId(transaction::Context::makeTransactionId());

            MUTEX_LOCKER(guard, _contextLock);
            // will create blocker entry for dbName if it doesn't exist
            auto& entry = _blockers[vocbase.name()];
            [[maybe_unused]] rocksdb::SequenceNumber blockerSeq =
                rcoll->placeRevisionTreeBlocker(trxId);
            entry[collection.id()] = trxId.id();
            blockerGuard.cancel();
            ++created;
          });
    }
  };

  // order blockers for all collections in the inventory
  TRI_ASSERT(inventory.slice().isObject());
  if (global) {
    for (auto it : VPackObjectIterator(inventory.slice())) {
      std::string dbName = it.key.copyString();
      VPackSlice collections = it.value.get("collections");
      handleCollections(dbName, collections);
    }
    result.add(inventory.slice());
  } else {
    handleCollections(vocbase.name(), inventory.slice().get("collections"));
    // add inventory data to result
    for (auto it : VPackObjectIterator(inventory.slice())) {
      result.add(it.key.copyString(), it.value);
    }
  }
  double ttl{};
  {
    MUTEX_LOCKER(locker, _contextLock);
    lazyCreateSnapshot();
    ttl = _ttl;
  }

  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);
  vocbase.replicationClients().track(syncerId(), replicationClientServerId(),
                                     clientInfo(), _snapshotTick, ttl);

  return Result();
}

// returns a stripped down version of the inventory, used for shard
// synchronization only
Result RocksDBReplicationContext::getInventory(
    TRI_vocbase_t& vocbase, std::string const& collectionName,
    VPackBuilder& result) {
  {
    MUTEX_LOCKER(locker, _contextLock);
    lazyCreateSnapshot();
  }

  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  // add a "collections" array with just our collection
  result.add("collections", VPackValue(VPackValueType::Array));

  ExecContext const& exec = ExecContext::current();
  if (exec.canUseCollection(vocbase.name(), collectionName, auth::Level::RO)) {
    auto collection = vocbase.lookupCollection(collectionName);
    if (collection != nullptr && !collection->deleted()) {
      // dump inventory data for collection/shard into result
      collection->toVelocyPackForInventory(result);
    }
  }

  result.close();  // collections

  // fake an empty "views" array here
  result.add("views", VPackValue(VPackValueType::Array));
  result.close();  // views

  vocbase.replicationClients().track(syncerId(), replicationClientServerId(),
                                     clientInfo(), _snapshotTick, _ttl);

  return Result();
}

void RocksDBReplicationContext::setPatchCount(std::string const& patchCount) {
  // _patchCount can only be set once in a context, and if it is set, it should
  // be non-empty. in addition, it should be set before we acquire the snapshot
  TRI_ASSERT(_snapshot == nullptr);
  TRI_ASSERT(!patchCount.empty());
  TRI_ASSERT(_patchCount.empty());
  _patchCount = patchCount;
}

std::string const& RocksDBReplicationContext::patchCount() const {
  return _patchCount;
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationContext::DumpResult RocksDBReplicationContext::dumpJson(
    TRI_vocbase_t& vocbase, std::string const& cname,
    basics::StringBuffer& buff, uint64_t chunkSize, bool useEnvelope) {
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&]() noexcept {
    try {
      releaseDumpIterator(cIter);
    } catch (std::exception const& ex) {
      LOG_TOPIC("2a670", ERR, Logger::REPLICATION)
          << "Failed to release dump iterator: " << ex.what();
    }
  });

  {
    DataSourceId const cid = ::normalizeIdentifier(vocbase, cname);
    if (cid.empty()) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }

    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter =
        getCollectionIterator(vocbase, cid, /*sorted*/ false, /*create*/ true);
    if (!cIter || cIter->sorted() || !cIter->iter) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
  }

  TRI_ASSERT(cIter->bounds().columnFamily() ==
             RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::Documents));

  RocksDBBlockerGuard blocker(cIter->logical.get());
  auto blockerSeq = blocker.placeBlocker();

  basics::VPackStringBufferAdapter adapter(buff.stringBuffer());
  velocypack::Dumper dumper(&adapter, &cIter->vpackOptions);
  TRI_ASSERT(cIter->iter && !cIter->sorted());
  while (cIter->hasMore() && buff.length() < chunkSize) {
    if (useEnvelope) {
      buff.appendText("{\"type\":");
      buff.appendInteger(REPLICATION_MARKER_DOCUMENT);  // set type
      buff.appendText(",\"data\":");
    }
    // printing the data, note: we need the CustomTypeHandler here
    dumper.dump(velocypack::Slice(
        reinterpret_cast<uint8_t const*>(cIter->iter->value().data())));
    if (useEnvelope) {
      buff.appendChar('}');
    }
    buff.appendChar('\n');
    cIter->iter->Next();
    ++cIter->numberDocumentsDumped;
  }

  bool hasMore = cIter->hasMore();
  if (hasMore) {
    cIter->currentTick++;
  } else if (cIter->documentCountAdjustmentTicket > 0) {
    // reached the end
    int64_t adjustment = cIter->numberDocumentsDumped - cIter->numberDocuments;
    handleCollectionCountAdjustment(cIter->documentCountAdjustmentTicket,
                                    adjustment, blockerSeq, cIter);

    cIter->numberDocumentsDumped = 0;
  }
  return DumpResult(TRI_ERROR_NO_ERROR, hasMore, cIter->currentTick);
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationContext::DumpResult RocksDBReplicationContext::dumpVPack(
    TRI_vocbase_t& vocbase, std::string const& cname,
    VPackBuffer<uint8_t>& buffer, uint64_t chunkSize, bool useEnvelope,
    bool singleArray) {
  TRI_ASSERT(!useEnvelope || !singleArray);

  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&]() noexcept {
    try {
      releaseDumpIterator(cIter);
    } catch (std::exception const& ex) {
      LOG_TOPIC("2b670", ERR, Logger::REPLICATION)
          << "Failed to release dump iterator: " << ex.what();
    }
  });

  {
    DataSourceId const cid = ::normalizeIdentifier(vocbase, cname);
    if (cid.empty()) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }

    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(chunkSize > 0);
    cIter =
        getCollectionIterator(vocbase, cid, /*sorted*/ false, /*create*/ true);
    if (!cIter || cIter->sorted() || !cIter->iter) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
  }

  RocksDBBlockerGuard blocker(cIter->logical.get());
  auto blockerSeq = blocker.placeBlocker();

  TRI_ASSERT(cIter->bounds().columnFamily() ==
             RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::Documents));

  VPackBuilder builder(buffer, &cIter->vpackOptions);
  if (singleArray) {
    // put everything into a single result array on demand
    builder.openArray(true);
  }
  TRI_ASSERT(cIter->iter && !cIter->sorted());
  while (cIter->hasMore() && buffer.length() < chunkSize) {
    if (useEnvelope) {
      builder.openObject();
      builder.add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
      builder.add(VPackValue("data"));
    }
    builder.add(velocypack::Slice(
        reinterpret_cast<uint8_t const*>(cIter->iter->value().data())));
    if (useEnvelope) {
      builder.close();
    }
    cIter->iter->Next();
    ++cIter->numberDocumentsDumped;
  }
  if (singleArray) {
    builder.close();
  }

  bool hasMore = cIter->hasMore();
  if (hasMore) {
    cIter->currentTick++;
  } else if (cIter->documentCountAdjustmentTicket > 0) {
    // reached the end
    int64_t adjustment = cIter->numberDocumentsDumped - cIter->numberDocuments;
    handleCollectionCountAdjustment(cIter->documentCountAdjustmentTicket,
                                    adjustment, blockerSeq, cIter);

    cIter->numberDocumentsDumped = 0;
  }
  return DumpResult(TRI_ERROR_NO_ERROR, hasMore, cIter->currentTick);
}

/// Dump all key chunks for the bound collection
Result RocksDBReplicationContext::dumpKeyChunks(TRI_vocbase_t& vocbase,
                                                DataSourceId cid,
                                                VPackBuilder& b,
                                                uint64_t chunkSize) {
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter]() noexcept {
    if (cIter) {
      cIter->release();
    }
  });

  if (cid.empty() || _snapshot == nullptr) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  {
    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(chunkSize > 0);
    cIter =
        getCollectionIterator(vocbase, cid, /*sorted*/ true, /*create*/ true);
    if (!cIter || !cIter->sorted() || !cIter->iter) {
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  TRI_ASSERT(cIter->lastSortedIteratorOffset == 0);
  TRI_ASSERT(cIter->bounds().columnFamily() ==
             RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::PrimaryIndex));

  RocksDBBlockerGuard blocker(cIter->logical.get());
  auto blockerSeq = blocker.placeBlocker();

  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  char ridBuffer[basics::maxUInt64StringSize];  // temporary buffer
                                                // for stringifying
                                                // revision ids
  RocksDBKey docKey;
  VPackBuilder tmpHashBuilder;
  rocksdb::TransactionDB* db = _engine.db();
  auto* rcoll =
      static_cast<RocksDBMetaCollection*>(cIter->logical->getPhysical());
  uint64_t const cObjectId = rcoll->objectId();
  uint64_t snapNumDocs = 0;

  b.openArray(true);
  while (cIter->hasMore()) {
    // needs to be strings because rocksdb::Slice gets invalidated
    std::string lowKey, highKey;
    uint64_t hashval = 0x012345678;

    uint64_t k = chunkSize;
    while (k-- > 0 && cIter->hasMore()) {
      snapNumDocs++;

      std::string_view key = RocksDBKey::primaryKey(cIter->iter->key());
      if (lowKey.empty()) {
        lowKey.assign(key.data(), key.size());
      }
      highKey.assign(key.data(), key.size());

      RevisionId docRev;
      if (!RocksDBValue::revisionId(cIter->iter->value(), docRev)) {
        // for collections that do not have the revisionId in the value
        LocalDocumentId docId = RocksDBValue::documentId(cIter->iter->value());
        docKey.constructDocument(cObjectId, docId);

        rocksdb::PinnableSlice ps;
        auto s = db->Get(cIter->readOptions(),
                         RocksDBColumnFamilyManager::get(
                             RocksDBColumnFamilyManager::Family::Documents),
                         docKey.string(), &ps);
        if (s.ok()) {
          TRI_ASSERT(ps.size() > 0);
          docRev = RevisionId::fromSlice(
              VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));
        } else {
          LOG_TOPIC("32e3b", WARN, Logger::REPLICATION)
              << "inconsistent primary index, "
              << "did not find document with key " << key;
          TRI_ASSERT(false);
          return {TRI_ERROR_INTERNAL, "inconsistent primary index"};
        }
      }
      TRI_ASSERT(docRev.isSet());

      // we can get away with the fast hash function here, as key values are
      // restricted to strings
      tmpHashBuilder.clear();
      tmpHashBuilder.add(
          VPackValuePair(key.data(), key.size(), VPackValueType::String));
      hashval ^= tmpHashBuilder.slice().hashString();
      tmpHashBuilder.clear();
      tmpHashBuilder.add(docRev.toValuePair(ridBuffer));
      hashval ^= tmpHashBuilder.slice().hashString();

      cIter->iter->Next();
    };

    if (lowKey.empty()) {  // no new documents were found
      TRI_ASSERT(k == chunkSize);
      break;
    }
    TRI_ASSERT(!highKey.empty());
    b.add(VPackValue(VPackValueType::Object));
    b.add("low", VPackValue(lowKey));
    b.add("high", VPackValue(highKey));
    b.add("hash", VPackValue(std::to_string(hashval)));
    b.close();
  }
  b.close();

  // reset so the next caller can dumpKeys from arbitrary location
  cIter->resetToStart();
  cIter->lastSortedIteratorOffset = 0;

  if (cIter->documentCountAdjustmentTicket > 0) {
    int64_t adjustment = snapNumDocs - cIter->numberDocuments;
    handleCollectionCountAdjustment(cIter->documentCountAdjustmentTicket,
                                    adjustment, blockerSeq, cIter);
  }

  return {};
}

/// dump all keys from collection for incremental sync
Result RocksDBReplicationContext::dumpKeys(TRI_vocbase_t& vocbase,
                                           DataSourceId cid, VPackBuilder& b,
                                           size_t chunk, size_t chunkSize,
                                           std::string const& lowKey) {
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter]() noexcept {
    if (cIter) {
      cIter->release();
    }
  });

  if (cid.empty() || _snapshot == nullptr) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  {
    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(chunkSize > 0);
    cIter =
        getCollectionIterator(vocbase, cid, /*sorted*/ true, /*create*/ false);
    if (!cIter || !cIter->sorted() || !cIter->iter) {
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  TRI_ASSERT(cIter->bounds().columnFamily() ==
             RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::PrimaryIndex));

  // Position the iterator correctly
  if (chunk != 0 &&
      ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "It seems that your chunk / chunkSize "
            "combination is not valid - overflow"};
  }

  RocksDBKey tmpKey;
  size_t from = chunk * chunkSize;
  if (from != cIter->lastSortedIteratorOffset) {
    if (!lowKey.empty()) {
      tmpKey.constructPrimaryIndexValue(cIter->bounds().objectId(),
                                        std::string_view(lowKey));
      cIter->iter->Seek(tmpKey.string());
      cIter->lastSortedIteratorOffset = from;
      TRI_ASSERT(cIter->iter->Valid());
    } else {  // no low key supplied, we can not use seek, should not happen
              // >= 3.2
      if (from == 0 || !cIter->hasMore() ||
          from < cIter->lastSortedIteratorOffset) {
        cIter->resetToStart();
        cIter->lastSortedIteratorOffset = 0;
      }

      if (from > cIter->lastSortedIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - cIter->lastSortedIteratorOffset;
        uint64_t to = cIter->skipKeys(diff);  // = (chunk + 1) * chunkSize
        TRI_ASSERT(to == diff || !cIter->hasMore());
        cIter->lastSortedIteratorOffset += to;
      }

      // TRI_ASSERT(_lastIteratorOffset == from);
      if (cIter->lastSortedIteratorOffset != from) {
        return {
            TRI_ERROR_BAD_PARAMETER,
            "The parameters you provided lead to an invalid iterator offset."};
      }
    }
  }

  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  char ridBuffer[basics::maxUInt64StringSize];  // temporary buffer
                                                // for stringifying
                                                // revision ids
  rocksdb::TransactionDB* db = _engine.db();
  auto* rcoll =
      static_cast<RocksDBMetaCollection*>(cIter->logical->getPhysical());
  const uint64_t cObjectId = rcoll->objectId();

  b.openArray(true);
  while (chunkSize-- > 0 && cIter->hasMore()) {
    auto scope = scopeGuard([&]() noexcept {
      cIter->iter->Next();
      cIter->lastSortedIteratorOffset++;
    });

    RevisionId docRev;
    if (!RocksDBValue::revisionId(cIter->iter->value(), docRev)) {
      // for collections that do not have the revisionId in the value
      LocalDocumentId docId = RocksDBValue::documentId(cIter->iter->value());
      tmpKey.constructDocument(cObjectId, docId);

      rocksdb::PinnableSlice ps;
      auto s = db->Get(cIter->readOptions(),
                       RocksDBColumnFamilyManager::get(
                           RocksDBColumnFamilyManager::Family::Documents),
                       tmpKey.string(), &ps);
      if (s.ok()) {
        TRI_ASSERT(ps.size() > 0);
        docRev = RevisionId::fromSlice(
            VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));
      } else {
        std::string_view key = RocksDBKey::primaryKey(cIter->iter->key());
        LOG_TOPIC("41803", WARN, Logger::REPLICATION)
            << "inconsistent primary index, "
            << "did not find document with key " << key;
        TRI_ASSERT(false);
        return {TRI_ERROR_INTERNAL, "inconsistent primary index"};
      }
    }

    std::string_view docKey(RocksDBKey::primaryKey(cIter->iter->key()));
    b.openArray(true);
    b.add(velocypack::ValuePair(docKey.data(), docKey.size(),
                                velocypack::ValueType::String));
    b.add(docRev.toValuePair(ridBuffer));
    b.close();
  }
  b.close();

  return {};
}

/// dump keys and document for incremental sync
Result RocksDBReplicationContext::dumpDocuments(
    TRI_vocbase_t& vocbase, DataSourceId cid, VPackBuilder& b, size_t chunk,
    size_t chunkSize, size_t offsetInChunk, size_t maxChunkSize,
    std::string const& lowKey, VPackSlice const& ids) {
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter]() noexcept {
    if (cIter) {
      cIter->release();
    }
  });

  if (cid.empty() || _snapshot == nullptr) {
    return {TRI_ERROR_BAD_PARAMETER};
  }

  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  {
    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(chunkSize > 0);
    cIter =
        getCollectionIterator(vocbase, cid, /*sorted*/ true, /*create*/ true);
    if (!cIter || !cIter->sorted() || !cIter->iter) {
      return {TRI_ERROR_BAD_PARAMETER};
    }
  }

  TRI_ASSERT(cIter->bounds().columnFamily() ==
             RocksDBColumnFamilyManager::get(
                 RocksDBColumnFamilyManager::Family::PrimaryIndex));

  // Position the iterator must be reset to the beginning
  // after calls to dumpKeys moved it forwards
  if (chunk != 0 &&
      ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return {TRI_ERROR_BAD_PARAMETER,
            "It seems that your chunk / chunkSize combination is not "
            "valid - overflow"};
  }

  RocksDBKey tmpKey;
  size_t from = chunk * chunkSize;
  if (from != cIter->lastSortedIteratorOffset) {
    if (!lowKey.empty()) {
      tmpKey.constructPrimaryIndexValue(cIter->bounds().objectId(),
                                        std::string_view(lowKey));
      cIter->iter->Seek(tmpKey.string());
      cIter->lastSortedIteratorOffset = from;
      TRI_ASSERT(cIter->iter->Valid());
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !cIter->hasMore() ||
          from < cIter->lastSortedIteratorOffset) {
        cIter->resetToStart();
        cIter->lastSortedIteratorOffset = 0;
      }

      if (from > cIter->lastSortedIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - cIter->lastSortedIteratorOffset;
        uint64_t to = cIter->skipKeys(diff);  // = (chunk + 1) * chunkSize
        TRI_ASSERT(to == diff || !cIter->hasMore());
        cIter->lastSortedIteratorOffset += to;
      }

      // TRI_ASSERT(_lastIteratorOffset == from);
      if (cIter->lastSortedIteratorOffset != from) {
        return {
            TRI_ERROR_BAD_PARAMETER,
            "The parameters you provided lead to an invalid iterator offset."};
      }
    }
  }

  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  rocksdb::TransactionDB* db = _engine.db();
  auto* rcoll =
      static_cast<RocksDBMetaCollection*>(cIter->logical->getPhysical());
  const uint64_t cObjectId = rcoll->objectId();

  auto& buffer = b.bufferRef();
  bool hasMore = true;
  b.openArray(true);
  size_t oldPos = from;
  size_t offset = 0;

  for (VPackSlice it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      return {TRI_ERROR_BAD_PARAMETER};
    }

    if (!hasMore) {
      LOG_TOPIC("b34fe", ERR, Logger::REPLICATION)
          << "Not enough data at " << oldPos;
      b.close();
      return {TRI_ERROR_FAILED, "Not enough data"};
    }

    size_t newPos = from + it.getNumber<size_t>();
    if (newPos > oldPos) {
      uint64_t ignore = cIter->skipKeys(newPos - oldPos);
      TRI_ASSERT(ignore == newPos - oldPos || !cIter->hasMore());
      cIter->lastSortedIteratorOffset += ignore;
    }

    bool full = false;
    if (offset < offsetInChunk) {
      // skip over the initial few documents
      if ((hasMore = cIter->hasMore())) {
        b.add(VPackValue(VPackValueType::Null));
        cIter->iter->Next();
        hasMore = cIter->hasMore();
      }
    } else {
      if ((hasMore = cIter->hasMore())) {
        // for collections that do not have the revisionId in the value
        LocalDocumentId docId = RocksDBValue::documentId(cIter->iter->value());
        tmpKey.constructDocument(cObjectId, docId);

        rocksdb::PinnableSlice ps;
        auto s = db->Get(cIter->readOptions(),
                         RocksDBColumnFamilyManager::get(
                             RocksDBColumnFamilyManager::Family::Documents),
                         tmpKey.string(), &ps);
        if (s.ok()) {
          TRI_ASSERT(ps.size() > 0);
          TRI_ASSERT(VPackSlice(reinterpret_cast<uint8_t const*>(ps.data()))
                         .isObject());
          b.add(VPackSlice(reinterpret_cast<uint8_t const*>(ps.data())));
        } else {
          std::string_view key = RocksDBKey::primaryKey(cIter->iter->key());
          LOG_TOPIC("d79df", WARN, Logger::REPLICATION)
              << "inconsistent primary index, "
              << "did not find document with key " << key;
          TRI_ASSERT(false);
          return {TRI_ERROR_INTERNAL, "inconsistent primary index"};
        }
        cIter->iter->Next();
        hasMore = cIter->hasMore();
      }

      if (buffer.byteSize() > maxChunkSize) {
        // result is big enough so that we abort prematurely
        full = true;
      }
    }

    cIter->lastSortedIteratorOffset++;
    oldPos = newPos + 1;
    ++offset;
    if (full) {
      break;
    }
  }
  b.close();  // b.openArray(true);

  return {};
}

void RocksDBReplicationContext::handleCollectionCountAdjustment(
    uint64_t documentCountAdjustmentTicket, int64_t adjustment,
    rocksdb::SequenceNumber blockerSeq, CollectionIterator* cIter) {
  TRI_ASSERT(cIter != nullptr);
  TRI_ASSERT(documentCountAdjustmentTicket > 0);

  if (adjustment != 0) {
    auto& vocbase = cIter->logical->vocbase();
    auto* rcoll =
        static_cast<RocksDBMetaCollection*>(cIter->logical->getPhysical());

    LOG_TOPIC("4986d", WARN, Logger::REPLICATION)
        << "inconsistent collection count detected for " << vocbase.name()
        << "/" << cIter->logical->name() << ", an offset of " << adjustment
        << " will be applied";
    auto adjustSeq = _engine.db()->GetLatestSequenceNumber();
    TRI_ASSERT(adjustSeq >= blockerSeq);
    if (adjustSeq <= blockerSeq) {
      adjustSeq = ::forceWrite(_engine);
      TRI_ASSERT(adjustSeq > blockerSeq);
    }
    rcoll->meta().adjustNumberDocumentsWithTicket(documentCountAdjustmentTicket,
                                                  adjustSeq, RevisionId::none(),
                                                  adjustment);
  }
}

double RocksDBReplicationContext::expires() const {
  MUTEX_LOCKER(locker, _contextLock);
  return _expires;
}

void RocksDBReplicationContext::extendLifetime(double ttl) {
  double now = TRI_microtime();

  MUTEX_LOCKER(locker, _contextLock);

  ttl = std::max(_ttl, ttl);
  TRI_ASSERT(ttl > 0.0);
  _expires = now + ttl;

  // make sure the WAL files are not deleted
  std::set<TRI_vocbase_t*> dbs;
  for (auto& pair : _iterators) {
    dbs.emplace(&pair.second->vocbase);
  }
  for (TRI_vocbase_t* vocbase : dbs) {
    vocbase->replicationClients().track(syncerId(), replicationClientServerId(),
                                        clientInfo(), _snapshotTick, ttl);
  }
}

/// create rocksdb snapshot, must hold _contextLock
void RocksDBReplicationContext::lazyCreateSnapshot() {
  _contextLock.assertLockedByCurrentThread();
  if (_snapshot == nullptr) {
    _snapshot = std::make_shared<rocksdb::ManagedSnapshot>(_engine.db());

    TRI_ASSERT(_snapshot != nullptr);
    TRI_ASSERT(_snapshot->snapshot() != nullptr);
    _snapshotTick = _snapshot->snapshot()->GetSequenceNumber();
  }
}

RocksDBReplicationContext::CollectionIterator::CollectionIterator(
    TRI_vocbase_t& vocbase, std::shared_ptr<LogicalCollection> const& coll,
    bool sorted, std::shared_ptr<rocksdb::ManagedSnapshot> snapshot)
    : vocbase(vocbase),
      logical{coll},
      iter{nullptr},
      _snapshot(std::move(snapshot)),
      _bounds{RocksDBKeyBounds::Empty()},
      currentTick{1},
      lastSortedIteratorOffset{0},
      vpackOptions{Options::Defaults},
      numberDocuments{0},
      numberDocumentsDumped{0},
      documentCountAdjustmentTicket{0},
      _resolver(vocbase),
      _cTypeHandler{},
      _readOptions{},
      _isUsed{false},
      _sortedIterator{!sorted}  // this makes sure that setSorted works
{
  TRI_ASSERT(coll != nullptr);
  TRI_ASSERT(_snapshot != nullptr);
  TRI_ASSERT(_snapshot->snapshot() != nullptr);

  _readOptions.snapshot = _snapshot->snapshot();
  _readOptions.verify_checksums = false;
  _readOptions.fill_cache = false;
  _readOptions.prefix_same_as_start = true;

  _cTypeHandler =
      transaction::Context::createCustomTypeHandler(vocbase, _resolver);
  vpackOptions.customTypeHandler = _cTypeHandler.get();
  vpackOptions.paddingBehavior =
      velocypack::Options::PaddingBehavior::UsePadding;
  setSorted(sorted);

  if (!vocbase.use()) {  // false if vobase was deleted
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  try {
    vocbase.useCollection(logical->id(), true);
  } catch (...) {
    vocbase.release();
    throw;
  }

  LOG_TOPIC("71236", TRACE, Logger::REPLICATION)
      << "replication created iterator for collection '" << coll->name() << "'";
}

RocksDBReplicationContext::CollectionIterator::~CollectionIterator() {
  TRI_ASSERT(!vocbase.isDangling());
  vocbase.releaseCollection(logical.get());
  LOG_TOPIC("71237", TRACE, Logger::REPLICATION)
      << "replication released iterator for collection '" << logical->name()
      << "'";
  logical.reset();
  vocbase.release();
}

void RocksDBReplicationContext::CollectionIterator::setSorted(bool sorted) {
  if (_sortedIterator != sorted) {
    iter.reset();
    _sortedIterator = sorted;

    if (sorted) {
      auto index = logical->getPhysical()->lookupIndex(
          IndexId::primary());  // RocksDBCollection->primaryIndex() is private
      TRI_ASSERT(index->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
      auto primaryIndex = static_cast<RocksDBPrimaryIndex*>(index.get());
      _bounds = RocksDBKeyBounds::PrimaryIndex(primaryIndex->objectId());
    } else {
      auto* rcoll = static_cast<RocksDBMetaCollection*>(logical->getPhysical());
      _bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
    }
    _upperLimit = _bounds.end();
    rocksdb::ColumnFamilyHandle* cf = _bounds.columnFamily();
    _cmp = cf->GetComparator();

    TRI_ASSERT(_upperLimit.size() > 0);
    _readOptions.iterate_upper_bound = &_upperLimit;
    iter.reset(vocbase.server()
                   .getFeature<EngineSelectorFeature>()
                   .engine<RocksDBEngine>()
                   .db()
                   ->NewIterator(_readOptions, cf));
    TRI_ASSERT(iter);
    iter->Seek(_bounds.start());
    currentTick = 1;
  }
}

// iterator convenience methods

bool RocksDBReplicationContext::CollectionIterator::hasMore() const {
  return iter->Valid() && !outOfRange();
}

bool RocksDBReplicationContext::CollectionIterator::outOfRange() const {
  TRI_ASSERT(_cmp != nullptr);
  return _cmp->Compare(iter->key(), _bounds.end()) > 0;
}

uint64_t RocksDBReplicationContext::CollectionIterator::skipKeys(
    uint64_t toSkip) {
  size_t skipped = 0;
  while (toSkip-- > 0 && this->hasMore()) {
    this->iter->Next();
    skipped++;
  }
  return skipped;
}

void RocksDBReplicationContext::CollectionIterator::resetToStart() {
  iter->Seek(_bounds.start());
}

RocksDBReplicationContext::CollectionIterator*
RocksDBReplicationContext::getCollectionIterator(TRI_vocbase_t& vocbase,
                                                 DataSourceId cid, bool sorted,
                                                 bool allowCreate) {
  _contextLock.assertLockedByCurrentThread();
  lazyCreateSnapshot();

  CollectionIterator* cIter{nullptr};
  // check if iterator already exists
  auto it = _iterators.find(cid);

  if (_iterators.end() != it) {
    // exists, check if used
    if (!it->second->isUsed()) {
      // unused, select it
      cIter = it->second.get();
    }
  } else {
    if (!allowCreate) {
      return nullptr;
    }

    // try to create one
    std::shared_ptr<LogicalCollection> logical{vocbase.lookupCollection(cid)};

    if (nullptr != logical) {
      auto result =
          _iterators.try_emplace(cid, std::make_unique<CollectionIterator>(
                                          vocbase, logical, sorted, _snapshot));

      if (result.second) {
        cIter = result.first->second.get();

        if (nullptr == cIter->iter) {
          cIter = nullptr;
          _iterators.erase(cid);
        }
      }
    }
  }

  if (cIter) {
    TRI_ASSERT(cIter->vocbase.id() == vocbase.id());
    cIter->use();

    if (allowCreate && cIter->sorted() != sorted) {
      cIter->setSorted(sorted);
    }
    // we are inserting the current tick (WAL sequence number) here.
    // this is ok because the batch creation is the first operation done
    // for initial synchronization. the inventory request and collection
    // dump requests will all happen after the batch creation, so the
    // current tick value here is good
    cIter->vocbase.replicationClients().track(
        syncerId(), replicationClientServerId(), clientInfo(), _snapshotTick,
        _ttl);
  }

  return cIter;
}

void RocksDBReplicationContext::releaseDumpIterator(CollectionIterator* it) {
  if (it) {
    TRI_ASSERT(it->isUsed());
    if (!it->hasMore()) {
      MUTEX_LOCKER(locker, _contextLock);
      it->vocbase.replicationClients().track(syncerId(),
                                             replicationClientServerId(),
                                             clientInfo(), _snapshotTick, _ttl);
      _iterators.erase(it->logical->id());
    } else {  // Context::release() will update the replication client
      it->release();
    }
  }
}

std::unique_ptr<containers::RevisionTree>
RocksDBReplicationContext::getPrefetchedRevisionTree(
    std::string const& collectionGuid) {
  if (collectionGuid == _collectionGuidOfPrefetchedRevisionTree) {
    LOG_TOPIC("456aa", DEBUG, Logger::ENGINES)
        << "Delivering prefetched revision tree for collection "
        << collectionGuid;
    _collectionGuidOfPrefetchedRevisionTree.clear();
    return std::move(_prefetchedRevisionTree);
  }
  LOG_TOPIC("456ab", DEBUG, Logger::ENGINES)
      << "No prefetched revision tree available for collection "
      << collectionGuid;
  return {nullptr};
}
