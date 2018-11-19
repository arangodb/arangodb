////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Logger/Logger.h"
#include "Replication/common-defines.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "Replication/utilities.h"
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

namespace {

TRI_voc_cid_t normalizeIdentifier(TRI_vocbase_t& vocbase,
                                  std::string const& identifier) {
  TRI_voc_cid_t id{0};
  std::shared_ptr<LogicalCollection> logical{
   vocbase.lookupCollection(identifier)
  };

  if (logical) {
    id = logical->id();
  }

  return id;
}

}  // namespace

RocksDBReplicationContext::RocksDBReplicationContext(double ttl,
                                                     TRI_server_id_t serverId)
    : _serverId{serverId},
      _id{TRI_NewTickServer()},
      _snapshotTick{0},
      _snapshot{nullptr},
      _ttl{ttl > 0.0 ? ttl : replutils::BatchInfo::DefaultTimeout},
      _expires{TRI_microtime() + _ttl},
      _isDeleted{false},
      _users{1} {}

RocksDBReplicationContext::~RocksDBReplicationContext() {
  MUTEX_LOCKER(guard, _contextLock);
  _iterators.clear();
  if (_snapshot != nullptr) {
    globalRocksDB()->ReleaseSnapshot(_snapshot);
    _snapshot = nullptr;
  }
}

TRI_voc_tick_t RocksDBReplicationContext::id() const { return _id; }

uint64_t RocksDBReplicationContext::snapshotTick() {
  MUTEX_LOCKER(locker, _contextLock);
  lazyCreateSnapshot();
  TRI_ASSERT(_snapshot != nullptr);
  return _snapshot->GetSequenceNumber();
}

/// invalidate all iterators with that vocbase
void RocksDBReplicationContext::removeVocbase(TRI_vocbase_t& vocbase) {
  bool found = false;

  MUTEX_LOCKER(locker, _contextLock);
  
  auto it = _iterators.begin();
  while (it != _iterators.end()) {
    if (it->second->vocbase.id() == vocbase.id()) {
      if (it->second->isUsed()) {
        LOG_TOPIC(ERR, Logger::REPLICATION) << "trying to delete used context";
      } else {
        found = true;
        it = _iterators.erase(it);
      }
    } else {
      it++;
    }
  }
  if (_iterators.empty() && found) {
    _isDeleted = true; // setDeleted also gets the lock
  }
}

/// remove matching iterator
void RocksDBReplicationContext::releaseIterators(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid) {
  MUTEX_LOCKER(locker, _contextLock);
  auto it = _iterators.find(cid);
  if (it != _iterators.end()) {
    if (it->second->isUsed()) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "trying to delete used iterator";
    } else {
      _iterators.erase(it);
    }
  } else {
    LOG_TOPIC(ERR, Logger::REPLICATION) << "trying to delete non-existent iterator";
  }
}

/// Bind collection for incremental sync
std::tuple<Result, TRI_voc_cid_t, uint64_t>
RocksDBReplicationContext::bindCollectionIncremental(TRI_vocbase_t& vocbase,
                                                     std::string const& cname) {
  
  std::shared_ptr<LogicalCollection> logical{
    vocbase.lookupCollection(cname)
  };
  
  if (!logical) {
    return std::make_tuple(TRI_ERROR_BAD_PARAMETER, 0, 0);
  }
  TRI_voc_cid_t cid = logical->id();
  
  MUTEX_LOCKER(writeLocker, _contextLock);
  
  auto it = _iterators.find(cid);
  if (it != _iterators.end()) { // nothing to do here
    return std::make_tuple(Result{}, it->second->logical->id(),
                           it->second->numberDocuments);
  }
  
  uint64_t numberDocuments;
  bool isNumberDocsExclusive = false;
  auto* rcoll = static_cast<RocksDBCollection*>(logical->getPhysical());
  if (_snapshot == nullptr) {
    // fetch number docs and snapshot under exclusive lock
    // this should enable us to correct the count later
    auto lockGuard = scopeGuard([rcoll] { rcoll->unlockWrite(); });
    int res = rcoll->lockWrite(transaction::Options::defaultLockTimeout);
    if (res != TRI_ERROR_NO_ERROR) {
      lockGuard.cancel();
      return std::make_tuple(res, 0, 0);
    }
    
    numberDocuments = rcoll->numberDocuments();
    isNumberDocsExclusive = true;
    lazyCreateSnapshot();
    lockGuard.fire(); // release exclusive lock
  } else { // fetch non-exclusive
    numberDocuments = rcoll->numberDocuments();
    lazyCreateSnapshot();
  }
  TRI_ASSERT(_snapshot != nullptr);

  auto iter = std::make_unique<CollectionIterator>(vocbase, logical, true, _snapshot);
  auto result = _iterators.emplace(cid, std::move(iter));
  TRI_ASSERT(result.second);
  
  CollectionIterator* cIter = result.first->second.get();
  if (nullptr == cIter->iter) {
    _iterators.erase(cid);
    return std::make_tuple(Result(TRI_ERROR_INTERNAL, "could not create db iterators"), 0, 0);
  }
  cIter->numberDocuments = numberDocuments;
  cIter->isNumberDocumentsExclusive = isNumberDocsExclusive;
  
  // we should have a valid iterator if there are documents in here
  TRI_ASSERT(numberDocuments == 0 || cIter->hasMore());
  return std::make_tuple(Result{}, cid, numberDocuments);
}

// returns inventory
Result RocksDBReplicationContext::getInventory(TRI_vocbase_t& vocbase,
                                               bool includeSystem, bool global,
                                               VPackBuilder& result) {
  {
    MUTEX_LOCKER(locker, _contextLock);
    lazyCreateSnapshot();
  }
  
  auto nameFilter = [includeSystem](LogicalCollection const* collection) {
    std::string const cname = collection->name();
    if (!includeSystem && !cname.empty() && cname[0] == '_') {
      // exclude all system collections
      return false;
    }

    if (TRI_ExcludeCollectionReplication(cname, includeSystem)) {
      // collection is excluded from replication
      return false;
    }

    // all other cases should be included
    return true;
  };
  
  TRI_ASSERT(_snapshot != nullptr);
  // FIXME is it technically correct to include newly created collections ?
  // simon: I think no, but this has been the behaviour since 3.2
  TRI_voc_tick_t tick = TRI_NewTickServer(); // = _lastArangoTick
  if (global) {
    // global inventory
    DatabaseFeature::DATABASE->inventory(result, tick, nameFilter);
  } else {
    // database-specific inventory
    vocbase.inventory(result, tick, nameFilter);
  }
  vocbase.updateReplicationClient(replicationClientId(), _snapshotTick, _ttl);

  return Result();
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationContext::DumpResult
  RocksDBReplicationContext::dumpJson(
    TRI_vocbase_t& vocbase, std::string const& cname,
    basics::StringBuffer& buff, uint64_t chunkSize) {
  TRI_ASSERT(_users > 0);
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&]{
    releaseDumpIterator(cIter);
  });

  {
    TRI_voc_cid_t const cid = ::normalizeIdentifier(vocbase, cname);
    if (0 == cid) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
    
    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter = getCollectionIterator(vocbase, cid, /*sorted*/false, /*create*/true);
    if (!cIter || cIter->sorted() || !cIter->iter) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
  }

  TRI_ASSERT(cIter->bounds.columnFamily() == RocksDBColumnFamily::documents());

  arangodb::basics::VPackStringBufferAdapter adapter(buff.stringBuffer());
  VPackDumper dumper(&adapter, &cIter->vpackOptions);
  TRI_ASSERT(cIter->iter && !cIter->sorted());
  while (cIter->hasMore() && buff.length() < chunkSize) {
    buff.appendText("{\"type\":");
    buff.appendInteger(REPLICATION_MARKER_DOCUMENT); // set type
    buff.appendText(",\"data\":");
    // printing the data, note: we need the CustomTypeHandler here
    dumper.dump(velocypack::Slice(cIter->iter->value().data()));
    buff.appendText("}\n");
    cIter->iter->Next();
  }

  bool hasMore = cIter->hasMore();
  if (hasMore) {
    cIter->currentTick++;
  }
  return DumpResult(TRI_ERROR_NO_ERROR, hasMore, cIter->currentTick);
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationContext::DumpResult
  RocksDBReplicationContext::dumpVPack(TRI_vocbase_t& vocbase, std::string const& cname,
                                       VPackBuffer<uint8_t>& buffer, uint64_t chunkSize) {
  TRI_ASSERT(_users > 0 && chunkSize > 0);

  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&]{
    releaseDumpIterator(cIter);
  });

  {
    TRI_voc_cid_t const cid = ::normalizeIdentifier(vocbase, cname);
    if (0 == cid) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
    
    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter = getCollectionIterator(vocbase, cid, /*sorted*/false, /*create*/true);
    if (!cIter || cIter->sorted() || !cIter->iter) {
      return DumpResult(TRI_ERROR_BAD_PARAMETER);
    }
  }

  TRI_ASSERT(cIter->bounds.columnFamily() == RocksDBColumnFamily::documents());
  
  VPackBuilder builder(buffer, &cIter->vpackOptions);
  TRI_ASSERT(cIter->iter && !cIter->sorted());
  while (cIter->hasMore() && buffer.length() < chunkSize) {
    builder.openObject();
    builder.add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
    builder.add(VPackValue("data"));
    builder.add(velocypack::Slice(cIter->iter->value().data()));
    builder.close();
    cIter->iter->Next();
  }
  
  bool hasMore = cIter->hasMore();
  if (hasMore) {
    cIter->currentTick++;
  }
  return DumpResult(TRI_ERROR_NO_ERROR, hasMore, cIter->currentTick);
}

/// Dump all key chunks for the bound collection
arangodb::Result RocksDBReplicationContext::dumpKeyChunks(TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
                                                          VPackBuilder& b, uint64_t chunkSize) {
  TRI_ASSERT(_users > 0 && chunkSize > 0);
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter] {
    if (cIter) {
      cIter->release();
    }
  });
  
  Result rv;
  {
    if (0 == cid || _snapshot == nullptr) {
      return RocksDBReplicationResult{TRI_ERROR_BAD_PARAMETER, _snapshotTick};
    }
    
    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter = getCollectionIterator(vocbase, cid, /*sorted*/true, /*create*/true);
    if (!cIter || !cIter->sorted() ||  !cIter->iter) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _snapshotTick);
    }
  }
  
  TRI_ASSERT(cIter->lastSortedIteratorOffset == 0);
  TRI_ASSERT(cIter->bounds.columnFamily() == RocksDBColumnFamily::primary());
  
  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  char ridBuffer[21]; // temporary buffer for stringifying revision ids
  RocksDBKey docKey;
  VPackBuilder tmpHashBuilder;
  rocksdb::TransactionDB* db = globalRocksDB();
  auto* rcoll = static_cast<RocksDBCollection*>(cIter->logical->getPhysical());
  const uint64_t cObjectId = rcoll->objectId();
  uint64_t snapNumDocs = 0;
  
  b.openArray(true);
  while (cIter->hasMore()) {
    // needs to be a strings because rocksdb::Slice gets invalidated
    std::string lowKey, highKey;
    uint64_t hash = 0x012345678;
    
    uint64_t k = chunkSize;
    while (k-- > 0 && cIter->hasMore()) {
      snapNumDocs++;
      
      StringRef key = RocksDBKey::primaryKey(cIter->iter->key());
      if (lowKey.empty()) {
        lowKey.assign(key.data(), key.size());
      }
      highKey.assign(key.data(), key.size());
      
      TRI_voc_rid_t docRev;
      if (!RocksDBValue::revisionId(cIter->iter->value(), docRev)) {
        // for collections that do not have the revisionId in the value
        LocalDocumentId docId = RocksDBValue::documentId(cIter->iter->value());
        docKey.constructDocument(cObjectId, docId);
        
        rocksdb::PinnableSlice ps;
        auto s = db->Get(cIter->readOptions(), RocksDBColumnFamily::documents(),
                         docKey.string(), &ps);
        if (s.ok()) {
          TRI_ASSERT(ps.size() > 0);
          docRev = TRI_ExtractRevisionId(VPackSlice(ps.data()));
        } else {
          LOG_TOPIC(WARN, Logger::REPLICATION) << "inconsistent primary index, "
          << "did not find document with key " << key.toString();
          TRI_ASSERT(false);
          return rv.reset(TRI_ERROR_INTERNAL);
        }
      }
      TRI_ASSERT(docRev != 0);
      
      // we can get away with the fast hash function here, as key values are
      // restricted to strings
      tmpHashBuilder.clear();
      tmpHashBuilder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
      hash ^= tmpHashBuilder.slice().hashString();
      tmpHashBuilder.clear();
      tmpHashBuilder.add(TRI_RidToValuePair(docRev, &ridBuffer[0]));
      hash ^= tmpHashBuilder.slice().hashString();
      
      cIter->iter->Next();
    };
    
    if (lowKey.empty()) { // no new documents were found
      TRI_ASSERT(k == chunkSize);
      break;
    }
    TRI_ASSERT(!highKey.empty());
    b.add(VPackValue(VPackValueType::Object));
    b.add("low", VPackValue(lowKey));
    b.add("high", VPackValue(highKey));
    b.add("hash", VPackValue(std::to_string(hash)));
    b.close();
  }
  b.close();

  // reset so the next caller can dumpKeys from arbitrary location
  cIter->resetToStart();
  cIter->lastSortedIteratorOffset = 0;
  
  if (cIter->isNumberDocumentsExclusive) {
    int64_t adjustment = snapNumDocs - cIter->numberDocuments;
    if (adjustment != 0) {
      LOG_TOPIC(WARN, Logger::REPLICATION) << "inconsistent collection count detected, "
      << "an offet of " << adjustment << " will be applied";
      auto* rcoll = static_cast<RocksDBCollection*>(cIter->logical->getPhysical());
      rcoll->adjustNumberDocuments(static_cast<TRI_voc_rid_t>(0), adjustment);
    }
  }

  return rv;
}

/// dump all keys from collection for incremental sync
arangodb::Result RocksDBReplicationContext::dumpKeys(
    TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
    VPackBuilder& b, size_t chunk, size_t chunkSize,
    std::string const& lowKey) {
  TRI_ASSERT(_users > 0 && chunkSize > 0);
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter] {
    if (cIter) {
      cIter->release();
    }
  });
  
  Result rv;
  {
    if (0 == cid || _snapshot == nullptr) {
      return RocksDBReplicationResult{TRI_ERROR_BAD_PARAMETER, _snapshotTick};
    }
    
    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter = getCollectionIterator(vocbase, cid, /*sorted*/true, /*create*/false);
    if (!cIter || !cIter->sorted() || !cIter->iter) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _snapshotTick);
    }
  }
  
  TRI_ASSERT(cIter->bounds.columnFamily() == RocksDBColumnFamily::primary());
  
  // Position the iterator correctly
  if (chunk != 0 && ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "It seems that your chunk / chunkSize "
                    "combination is not valid - overflow");
  }

  RocksDBKey tmpKey;
  size_t from = chunk * chunkSize;
  if (from != cIter->lastSortedIteratorOffset) {
    if (!lowKey.empty()) {
      tmpKey.constructPrimaryIndexValue(cIter->bounds.objectId(), StringRef(lowKey));
      cIter->iter->Seek(tmpKey.string());
      cIter->lastSortedIteratorOffset = from;
      TRI_ASSERT(cIter->iter->Valid());
    } else {  // no low key supplied, we can not use seek, should not happen >= 3.2
      if (from == 0 || !cIter->hasMore() || from < cIter->lastSortedIteratorOffset) {
        cIter->resetToStart();
        cIter->lastSortedIteratorOffset = 0;
      }

      if (from > cIter->lastSortedIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - cIter->lastSortedIteratorOffset;
        uint64_t to = cIter->skipKeys(diff); // = (chunk + 1) * chunkSize
        TRI_ASSERT(to == diff || !cIter->hasMore());
        cIter->lastSortedIteratorOffset += to;
      }

      // TRI_ASSERT(_lastIteratorOffset == from);
      if (cIter->lastSortedIteratorOffset != from) {
        return rv.reset(
            TRI_ERROR_BAD_PARAMETER,
            "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }

  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  char ridBuffer[21]; // temporary buffer for stringifying revision ids
  rocksdb::TransactionDB* db = globalRocksDB();
  auto* rcoll = static_cast<RocksDBCollection*>(cIter->logical->getPhysical());
  const uint64_t cObjectId = rcoll->objectId();

  b.openArray(true);
  while (chunkSize-- > 0 && cIter->hasMore()) {
    auto scope = scopeGuard([&]{
      cIter->iter->Next();
      cIter->lastSortedIteratorOffset++;
    });
    
    TRI_voc_rid_t docRev;
    if (!RocksDBValue::revisionId(cIter->iter->value(), docRev)) {
      // for collections that do not have the revisionId in the value
      LocalDocumentId docId = RocksDBValue::documentId(cIter->iter->value());
      tmpKey.constructDocument(cObjectId, docId);
      
      rocksdb::PinnableSlice ps;
      auto s = db->Get(cIter->readOptions(), RocksDBColumnFamily::documents(),
                       tmpKey.string(), &ps);
      if (s.ok()) {
        TRI_ASSERT(ps.size() > 0);
        docRev = TRI_ExtractRevisionId(VPackSlice(ps.data()));
      } else {
        StringRef key = RocksDBKey::primaryKey(cIter->iter->key());
        LOG_TOPIC(WARN, Logger::REPLICATION) << "inconsistent primary index, "
        << "did not find document with key " << key.toString();
        TRI_ASSERT(false);
        return rv.reset(TRI_ERROR_INTERNAL);
      }
    }
    
    StringRef docKey(RocksDBKey::primaryKey(cIter->iter->key()));
    b.openArray(true);
    b.add(velocypack::ValuePair(docKey.data(), docKey.size(), velocypack::ValueType::String));
    b.add(TRI_RidToValuePair(docRev, &ridBuffer[0]));
    b.close();
  }
  b.close();

  return rv;
}

/// dump keys and document for incremental sync
arangodb::Result RocksDBReplicationContext::dumpDocuments(
    TRI_vocbase_t& vocbase, TRI_voc_cid_t cid,
    VPackBuilder& b, size_t chunk, size_t chunkSize, size_t offsetInChunk,
    size_t maxChunkSize, std::string const& lowKey, VPackSlice const& ids) {
  TRI_ASSERT(_users > 0 && chunkSize > 0);
  CollectionIterator* cIter{nullptr};
  auto guard = scopeGuard([&cIter] {
    if (cIter) {
      cIter->release();
    }
  });
  
  Result rv;
  {
    if (0 == cid || _snapshot == nullptr) {
      return RocksDBReplicationResult{TRI_ERROR_BAD_PARAMETER, _snapshotTick};
    }
    
    MUTEX_LOCKER(writeLocker, _contextLock);
    cIter = getCollectionIterator(vocbase, cid, /*sorted*/true, /*create*/true);
    if (!cIter || !cIter->sorted() || !cIter->iter) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _snapshotTick);
    }
  }
  
  TRI_ASSERT(cIter->bounds.columnFamily() == RocksDBColumnFamily::primary());

  // Position the iterator must be reset to the beginning
  // after calls to dumpKeys moved it forwards
  if (chunk != 0 &&
      ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER,
                    "It seems that your chunk / chunkSize combination is not "
                    "valid - overflow");
  }
  
  RocksDBKey tmpKey;
  size_t from = chunk * chunkSize;
  if (from != cIter->lastSortedIteratorOffset) {
    if (!lowKey.empty()) {
      tmpKey.constructPrimaryIndexValue(cIter->bounds.objectId(), StringRef(lowKey));
      cIter->iter->Seek(tmpKey.string());
      cIter->lastSortedIteratorOffset = from;
      TRI_ASSERT(cIter->iter->Valid());
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !cIter->hasMore() || from < cIter->lastSortedIteratorOffset) {
        cIter->resetToStart();
        cIter->lastSortedIteratorOffset = 0;
      }
      
      if (from > cIter->lastSortedIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - cIter->lastSortedIteratorOffset;
        uint64_t to = cIter->skipKeys(diff); // = (chunk + 1) * chunkSize
        TRI_ASSERT(to == diff || !cIter->hasMore());
        cIter->lastSortedIteratorOffset += to;
      }
      
      // TRI_ASSERT(_lastIteratorOffset == from);
      if (cIter->lastSortedIteratorOffset != from) {
        return rv.reset(TRI_ERROR_BAD_PARAMETER,
                        "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }
  
  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  rocksdb::TransactionDB* db = globalRocksDB();
  auto* rcoll = static_cast<RocksDBCollection*>(cIter->logical->getPhysical());
  const uint64_t cObjectId = rcoll->objectId();
  
  auto buffer = b.buffer();
  bool hasMore = true;
  b.openArray(true);
  size_t oldPos = from;
  size_t offset = 0;
  
  for (auto const& it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      return rv.reset(TRI_ERROR_BAD_PARAMETER);
    }
    
    if (!hasMore) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "Not enough data at " << oldPos;
      b.close();
      return rv.reset(TRI_ERROR_FAILED, "Not enough data");
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
        auto s = db->Get(cIter->readOptions(), RocksDBColumnFamily::documents(),
                         tmpKey.string(), &ps);
        if (s.ok()) {
          TRI_ASSERT(ps.size() > 0);
          TRI_ASSERT(VPackSlice(ps.data()).isObject());
          b.add(VPackSlice(ps.data()));
        } else {
          StringRef key = RocksDBKey::primaryKey(cIter->iter->key());
          LOG_TOPIC(WARN, Logger::REPLICATION) << "inconsistent primary index, "
          << "did not find document with key " << key.toString();
          TRI_ASSERT(false);
          return Result(TRI_ERROR_INTERNAL);
        }
        cIter->iter->Next();
        hasMore = cIter->hasMore();
      }
      
      if (buffer->byteSize() > maxChunkSize) {
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
  b.close(); // b.openArray(true);

  return Result();
}

double RocksDBReplicationContext::expires() const {
  MUTEX_LOCKER(locker, _contextLock);
  return _expires;
}

bool RocksDBReplicationContext::isDeleted() const {
  MUTEX_LOCKER(locker, _contextLock);
  return _isDeleted;
}

void RocksDBReplicationContext::setDeleted() {
  MUTEX_LOCKER(locker, _contextLock);
  _isDeleted = true;
}

bool RocksDBReplicationContext::isUsed() const {
  MUTEX_LOCKER(locker, _contextLock);
  return (_users > 0);
}

void RocksDBReplicationContext::use(double ttl) {
  MUTEX_LOCKER(locker, _contextLock);
  TRI_ASSERT(!_isDeleted);

  ++_users;
  ttl = std::max(std::max(_ttl, ttl), replutils::BatchInfo::DefaultTimeout);
  _expires = TRI_microtime() + ttl;

  // make sure the WAL files are not deleted
  std::set<TRI_vocbase_t*> dbs;
  for (auto& pair : _iterators) {
    dbs.emplace(&pair.second->vocbase);
  }
  for (TRI_vocbase_t* vocbase : dbs) {
    vocbase->updateReplicationClient(replicationClientId(), _snapshotTick, ttl);
  }
}

void RocksDBReplicationContext::release() {
  MUTEX_LOCKER(locker, _contextLock);
  TRI_ASSERT(_users > 0);
  double ttl = std::max(_ttl, replutils::BatchInfo::DefaultTimeout);
  _expires = TRI_microtime() + ttl;
  --_users;

  TRI_ASSERT(_ttl > 0);
  // make sure the WAL files are not deleted immediately
  std::set<TRI_vocbase_t*> dbs;
  for (auto& pair : _iterators) {
    dbs.emplace(&pair.second->vocbase);
  }
  for (TRI_vocbase_t* vocbase : dbs) {
    vocbase->updateReplicationClient(replicationClientId(), _snapshotTick, ttl);
  }
}

/// extend without using the context
void RocksDBReplicationContext::extendLifetime(double ttl) {
  MUTEX_LOCKER(locker, _contextLock);
  ttl = std::max(std::max(_ttl, ttl), replutils::BatchInfo::DefaultTimeout);
  _expires = TRI_microtime() + ttl;
}

/// create rocksdb snapshot, must hold _contextLock
void RocksDBReplicationContext::lazyCreateSnapshot() {
  _contextLock.assertLockedByCurrentThread();
  if (_snapshot == nullptr) {
    _snapshot = globalRocksDB()->GetSnapshot();
    TRI_ASSERT(_snapshot);
    _snapshotTick = _snapshot->GetSequenceNumber();
  }
}

RocksDBReplicationContext::CollectionIterator::CollectionIterator(
    TRI_vocbase_t& vocbase,
    std::shared_ptr<LogicalCollection> const& coll,
    bool sorted, rocksdb::Snapshot const* snapshot)
    : vocbase(vocbase),
      logical{coll},
      iter{nullptr},
      bounds{RocksDBKeyBounds::Empty()},
      currentTick{1},
      lastSortedIteratorOffset{0},
      vpackOptions{Options::Defaults},
      numberDocuments{0},
      isNumberDocumentsExclusive{false},
      _resolver(vocbase),
      _cTypeHandler{},
      _readOptions{},
      _isUsed{false},
      _sortedIterator{!sorted} // this makes sure that setSorted works
{
  TRI_ASSERT(snapshot != nullptr && coll);
  _readOptions.snapshot = snapshot;
  _readOptions.verify_checksums = false;
  _readOptions.fill_cache = false;
  _readOptions.prefix_same_as_start = true;
  
  if (!vocbase.use()) { // false if vobase was deleted
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATABASE_NOT_FOUND);
  }
  TRI_vocbase_col_status_e ignore;
  int res = vocbase.useCollection(logical.get(), ignore);
  if (res != TRI_ERROR_NO_ERROR) { // collection was deleted
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }
  
  _cTypeHandler.reset(transaction::Context::createCustomTypeHandler(vocbase, _resolver));
  vpackOptions.customTypeHandler = _cTypeHandler.get();
  setSorted(sorted);
}

RocksDBReplicationContext::CollectionIterator::~CollectionIterator() {
  TRI_ASSERT(!vocbase.isDangling());
  vocbase.releaseCollection(logical.get());
  logical.reset();
  vocbase.release();
}

void RocksDBReplicationContext::CollectionIterator::setSorted(bool sorted) {
  if (_sortedIterator != sorted) {
    iter.reset();
    _sortedIterator = sorted;
    
    if (sorted) {
      auto index = logical->getPhysical()->lookupIndex(0); //RocksDBCollection->primaryIndex() is private
      TRI_ASSERT(index->type() == Index::IndexType::TRI_IDX_TYPE_PRIMARY_INDEX);
      auto primaryIndex = static_cast<RocksDBPrimaryIndex*>(index.get());
      bounds = RocksDBKeyBounds::PrimaryIndex(primaryIndex->objectId());
    } else {
      auto* rcoll = static_cast<RocksDBCollection*>(logical->getPhysical());
      bounds = RocksDBKeyBounds::CollectionDocuments(rcoll->objectId());
    }
    _upperLimit = bounds.end();
    rocksdb::ColumnFamilyHandle* cf = bounds.columnFamily();
    _cmp = cf->GetComparator();
    
    TRI_ASSERT(_upperLimit.size() > 0);
    _readOptions.iterate_upper_bound = &_upperLimit;
    iter.reset(rocksutils::globalRocksDB()->NewIterator(_readOptions, cf));
    TRI_ASSERT(iter);
    iter->Seek(bounds.start());
    currentTick = 1;
  }
}

// iterator convenience methods

bool RocksDBReplicationContext::CollectionIterator::hasMore() const {
  return iter->Valid() && !outOfRange();
}

bool RocksDBReplicationContext::CollectionIterator::outOfRange() const {
  TRI_ASSERT(_cmp != nullptr);
  return _cmp->Compare(iter->key(), bounds.end()) > 0;
}

uint64_t RocksDBReplicationContext::CollectionIterator::skipKeys(uint64_t toSkip) {
  size_t skipped = 0;
  while (toSkip-- > 0 && this->hasMore()) {
    this->iter->Next();
    skipped++;
  }
  return skipped;
}

void RocksDBReplicationContext::CollectionIterator::resetToStart() {
  iter->Seek(bounds.start());
}

RocksDBReplicationContext::CollectionIterator*
RocksDBReplicationContext::getCollectionIterator(TRI_vocbase_t& vocbase,
                                                 TRI_voc_cid_t cid,
                                                 bool sorted,
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
    std::shared_ptr<LogicalCollection> logical{
      vocbase.lookupCollection(cid)
    };

    if (nullptr != logical) {
      auto result = _iterators.emplace(cid,
        std::make_unique<CollectionIterator>(vocbase, logical, sorted, _snapshot));

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
    cIter->vocbase.updateReplicationClient(replicationClientId(), _snapshotTick, _ttl);
  }

  return cIter;
}

void RocksDBReplicationContext::releaseDumpIterator(CollectionIterator* it) {
  if (it) {
    TRI_ASSERT(it->isUsed());
    if (!it->hasMore()) {
      it->vocbase.updateReplicationClient(replicationClientId(), _snapshotTick, _ttl);
      MUTEX_LOCKER(locker, _contextLock);
      _iterators.erase(it->logical->id());
    } else { // Context::release() will update the replication client
      it->release();
    }
  }
}
