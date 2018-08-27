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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBReplicationContext.h"
#include "Basics/MutexLocker.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Logger/Logger.h"
#include "Replication/InitialSyncer.h"
#include "Replication/common-defines.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/StandaloneContext.h"
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

TRI_voc_cid_t normalizeIdentifier(transaction::Methods const& trx,
                                  std::string const& identifier) {
  TRI_voc_cid_t id{0};
  std::shared_ptr<LogicalCollection> logical{
    trx.vocbase().lookupCollection(identifier)
  };

  if (logical) {
    id = logical->id();
  }

  return id;
}

}  // namespace

RocksDBReplicationContext::RocksDBReplicationContext(TRI_vocbase_t* vocbase,
                                                     double ttl,
                                                     TRI_server_id_t serverId)
    : _vocbase{vocbase},
      _serverId{serverId},
      _id{TRI_NewTickServer()},
      _lastTick{0},
      _trx{},
      _collection{nullptr},
      _lastIteratorOffset{0},
      _ttl{ttl > 0.0 ? ttl : replutils::BatchInfo::DefaultTimeout},
      _expires{TRI_microtime() + _ttl},
      _isDeleted{false},
      _exclusive{true},
      _users{1} {}

RocksDBReplicationContext::~RocksDBReplicationContext() {
  releaseDumpingResources();
}

TRI_voc_tick_t RocksDBReplicationContext::id() const { return _id; }

uint64_t RocksDBReplicationContext::lastTick() const {
  MUTEX_LOCKER(locker, _contextLock);
  return _lastTick;
}

uint64_t RocksDBReplicationContext::count() const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_collection != nullptr);
  MUTEX_LOCKER(locker, _contextLock);
  RocksDBCollection* rcoll =
      toRocksDBCollection(_collection->logical.getPhysical());
  return rcoll->numberDocuments(_trx.get());
}

TRI_vocbase_t* RocksDBReplicationContext::vocbase() const {
  MUTEX_LOCKER(locker, _contextLock);

  if (!_guard) {
    return nullptr;
  }

  return &(_guard->database());
}

// creates new transaction/snapshot
void RocksDBReplicationContext::bind(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(_exclusive);
  internalBind(vocbase);
}

void RocksDBReplicationContext::internalBind(
    TRI_vocbase_t& vocbase,
    bool allowChange /*= true*/
) {
  if (!_trx || !_guard || (&(_guard->database()) != &vocbase)) {
    TRI_ASSERT(allowChange);
    rocksdb::Snapshot const* snap = nullptr;

    if (_trx) {
      auto state = RocksDBTransactionState::toState(_trx.get());
      snap = state->stealReadSnapshot();
      _trx->abort();
      _trx.reset();
    }

    releaseDumpingResources();

    _guard.reset(new DatabaseGuard(vocbase));
    transaction::Options transactionOptions;
    transactionOptions.waitForSync = false;
    transactionOptions.allowImplicitCollections = true;

    auto ctx = transaction::StandaloneContext::Create(vocbase);

    _trx.reset(
        new transaction::Methods(ctx, {}, {}, {}, transactionOptions));

    auto state = RocksDBTransactionState::toState(_trx.get());

    state->prepareForParallelReads();

    if (snap != nullptr) {
      state->donateSnapshot(snap);
      TRI_ASSERT(snap->GetSequenceNumber() == state->sequenceNumber());
    }

    Result res = _trx->begin();

    if (!res.ok()) {
      _guard.reset();
      THROW_ARANGO_EXCEPTION(res);
    }

    _lastTick = state->sequenceNumber();
  }
}

/// Bind collection for incremental sync
int RocksDBReplicationContext::bindCollectionIncremental(
    TRI_vocbase_t& vocbase,
    std::string const& collectionIdentifier) {
  TRI_ASSERT(_exclusive);
  TRI_ASSERT(nullptr != _trx);
  internalBind(vocbase);

  TRI_voc_cid_t const id{::normalizeIdentifier(*_trx, collectionIdentifier)};

  if (0 == id) {
    return TRI_ERROR_BAD_PARAMETER;
  }

  if ((nullptr == _collection) || (id != _collection->logical.id())) {
    MUTEX_LOCKER(writeLocker, _contextLock);

    if (_collection) {
      _collection->release();
    }

    _collection = getCollectionIterator(id, /*sorted*/ true);
    if (nullptr == _collection) {
      return TRI_ERROR_BAD_PARAMETER;
    }
  }

  return TRI_ERROR_NO_ERROR;
}

int RocksDBReplicationContext::chooseDatabase(TRI_vocbase_t& vocbase) {
  TRI_ASSERT(_users > 0);
  MUTEX_LOCKER(locker, _contextLock);

  if (&(_guard->database()) == &vocbase) {
    return TRI_ERROR_NO_ERROR;  // nothing to do here
  }

  // need to actually change it, first make sure we're alone in this context
  if (_users > 1) {
    return TRI_ERROR_CURSOR_BUSY;
  }

  // make the actual change
  internalBind(vocbase, true);

  return TRI_ERROR_NO_ERROR;
}

// returns inventory
Result RocksDBReplicationContext::getInventory(TRI_vocbase_t* vocbase,
                                               bool includeSystem, bool global,
                                               VPackBuilder& result) {
  TRI_ASSERT(vocbase != nullptr);
  if (!_trx) {
    return TRI_ERROR_BAD_PARAMETER;
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

  TRI_voc_tick_t tick = TRI_CurrentTickServer();
  if (global) {
    // global inventory
    DatabaseFeature::DATABASE->inventory(result, tick, nameFilter);
  } else {
    // database-specific inventory
    vocbase->inventory(result, tick, nameFilter);
  }
  return Result();
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationResult RocksDBReplicationContext::dumpJson(
    TRI_vocbase_t* vocbase, std::string const& cname,
    basics::StringBuffer& buff, uint64_t chunkSize) {
  TRI_ASSERT(_users > 0 && !_exclusive);
  CollectionIterator* collectionIter{nullptr};
  TRI_DEFER(if (collectionIter) {
    MUTEX_LOCKER(locker, _contextLock);
    collectionIter->release();
  });

  {
    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(vocbase != nullptr);

    if (!_trx || !_guard || (&(_guard->database()) != vocbase)) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
    }
    TRI_voc_cid_t const cid = ::normalizeIdentifier(*_trx, cname);
    if (0 == cid) {
      return RocksDBReplicationResult{TRI_ERROR_BAD_PARAMETER, _lastTick};
    }
    collectionIter = getCollectionIterator(cid, /*sorted*/false);
    if (!collectionIter) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
    }
  }

  TRI_ASSERT(collectionIter->iter->bounds().columnFamily() == RocksDBColumnFamily::documents());

  arangodb::basics::VPackStringBufferAdapter adapter(buff.stringBuffer());
  auto cb = [&collectionIter, &buff, &adapter](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    buff.appendText("{\"type\":");
    buff.appendInteger(REPLICATION_MARKER_DOCUMENT); // set type
    buff.appendText(",\"data\":");

    // printing the data, note: we need the CustomTypeHandler here
    VPackDumper dumper(&adapter, &collectionIter->vpackOptions);
    dumper.dump(velocypack::Slice(rocksValue.data()));
    buff.appendText("}\n");
    return true;
  };

  TRI_ASSERT(collectionIter->iter && !collectionIter->sorted());
  while (collectionIter->hasMore && buff.length() < chunkSize) {
    try {
      // limit is 1 so we can cancel right when we hit chunkSize
      collectionIter->hasMore = collectionIter->iter->next(cb, 1);
    } catch (std::exception const&) {
      collectionIter->hasMore = false;
      return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    } catch (RocksDBReplicationResult const& ex) {
      collectionIter->hasMore = false;
      return ex;
    }
  }

  if (collectionIter->hasMore) {
    collectionIter->currentTick++;
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, collectionIter->currentTick);
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationResult RocksDBReplicationContext::dumpVPack(TRI_vocbase_t* vocbase, std::string const& cname,
                                                              VPackBuffer<uint8_t>& buffer, uint64_t chunkSize) {
  TRI_ASSERT(_users > 0 && !_exclusive);
  CollectionIterator* collectionIter{nullptr};
  TRI_DEFER(if (collectionIter) {
    MUTEX_LOCKER(locker, _contextLock);
    collectionIter->release();
  });

  {
    MUTEX_LOCKER(writeLocker, _contextLock);
    TRI_ASSERT(vocbase != nullptr);

    if (!_trx || !_guard || (&(_guard->database()) != vocbase)) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
    }
    TRI_voc_cid_t const cid = ::normalizeIdentifier(*_trx, cname);
    if (0 == cid) {
      return RocksDBReplicationResult{TRI_ERROR_BAD_PARAMETER, _lastTick};
    }
    collectionIter = getCollectionIterator(cid, /*sorted*/false);
    if (!collectionIter) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
    }
  }

  TRI_ASSERT(collectionIter->iter->bounds().columnFamily() == RocksDBColumnFamily::documents());

  VPackBuilder builder(buffer, &collectionIter->vpackOptions);
  auto cb = [&builder](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    builder.openObject();
    builder.add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
    builder.add(VPackValue("data"));
    builder.add(velocypack::Slice(rocksValue.data()));
    builder.close();
    return true;
  };

  TRI_ASSERT(collectionIter->iter && !collectionIter->sorted());
  while (collectionIter->hasMore && buffer.length() < chunkSize) {
    try {
      // limit is 1 so we can cancel right when we hit chunkSize
      collectionIter->hasMore = collectionIter->iter->next(cb, 1);
    } catch (std::exception const&) {
      collectionIter->hasMore = false;
      return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    } catch (RocksDBReplicationResult const& ex) {
      collectionIter->hasMore = false;
      return ex;
    }
  }

  if (collectionIter->hasMore) {
    collectionIter->currentTick++;
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, collectionIter->currentTick);
}

/// Dump all key chunks for the bound collection
arangodb::Result RocksDBReplicationContext::dumpKeyChunks(VPackBuilder& b,
                                                          uint64_t chunkSize) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_collection->iter) {
    return rv.reset(
        TRI_ERROR_BAD_PARAMETER,
        "the replication context iterator has not been initialized");
  }
  _collection->setSorted(true, _trx.get());
  TRI_ASSERT(_collection->sorted() && _lastIteratorOffset == 0);
  
  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  // temporary buffer for stringifying revision ids
  char ridBuffer[21];

  std::string lowKey;
  std::string highKey;  // needs to be a string (not ref) as the rocksdb slice will not be valid outside the callback
  VPackBuilder builder;
  uint64_t hash = 0x012345678;

  auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    StringRef key = RocksDBKey::primaryKey(rocksKey);
    highKey.assign(key.data(), key.size());

    TRI_voc_rid_t docRev;
    if (!RocksDBValue::revisionId(rocksValue, docRev)) {
      // for collections that do not have the revisionId in the value
      auto documentId = RocksDBValue::documentId(rocksValue); // we want probably to do this instead
      if (false == _collection->logical.readDocumentWithCallback(_trx.get(), documentId, [&docRev](LocalDocumentId const&, VPackSlice doc) {
        docRev = TRI_ExtractRevisionId(doc);
      })) {
        TRI_ASSERT(false);
        return true;
      }
    }

    // set type
    if (lowKey.empty()) {
      lowKey.assign(key.data(), key.size());
    }

    // we can get away with the fast hash function here, as key values are
    // restricted to strings

    builder.clear();
    builder.add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
    hash ^= builder.slice().hashString();
    builder.clear();
    builder.add(TRI_RidToValuePair(docRev, &ridBuffer[0]));
    hash ^= builder.slice().hashString();

    return true;
  }; //cb

  b.openArray(true);
  while (_collection->hasMore) {
    try {
      _collection->hasMore = _collection->iter->next(cb, chunkSize);

      if (lowKey.empty()) {
        // if lowKey is empty, no new documents were found
        break;
      }
      b.add(VPackValue(VPackValueType::Object));
      b.add("low", VPackValue(lowKey));
      b.add("high", VPackValue(highKey));
      b.add("hash", VPackValue(std::to_string(hash)));
      b.close();
      lowKey.clear();      // reset string
      hash = 0x012345678;  // the next block ought to start with a clean sheet
    } catch (std::exception const& ex) {
      return rv.reset(TRI_ERROR_INTERNAL, ex.what());
    }
  }

  b.close();
  // we will not call this method twice
  _collection->iter->reset();
  _lastIteratorOffset = 0;

  return rv;
}

/// dump all keys from collection for incremental sync
arangodb::Result RocksDBReplicationContext::dumpKeys(
    VPackBuilder& b, size_t chunk, size_t chunkSize,
    std::string const& lowKey) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_collection->iter) {
    return rv.reset(
        TRI_ERROR_BAD_PARAMETER,
        "the replication context iterator has not been initialized");
  }
  _collection->setSorted(true, _trx.get());
  TRI_ASSERT(_collection->iter);
  TRI_ASSERT(_collection->sorted());

  auto primary = _collection->iter.get();

  // Position the iterator correctly
  if (chunk != 0 && ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER,
                    "It seems that your chunk / chunkSize combination is not "
                    "valid - overflow");
  }

  size_t from = chunk * chunkSize;

  if (from != _lastIteratorOffset) {
    if (!lowKey.empty()) {
      RocksDBKeyLeaser val(_trx.get());
      val->constructPrimaryIndexValue(primary->bounds().objectId(), StringRef(lowKey));
      primary->seek(val->string());
      _lastIteratorOffset = from;
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !_collection->hasMore || from < _lastIteratorOffset) {
        _collection->iter->reset();
        _lastIteratorOffset = 0;
      }

      if (from > _lastIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - _lastIteratorOffset;
        uint64_t to = 0;  // = (chunk + 1) * chunkSize;
        _collection->iter->skip(diff, to);
        _lastIteratorOffset += to;
      }

      // TRI_ASSERT(_lastIteratorOffset == from);
      if (_lastIteratorOffset != from) {
        return rv.reset(
            TRI_ERROR_BAD_PARAMETER,
            "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }

  // reserve some space in the result builder to avoid frequent reallocations
  b.reserve(8192);
  // temporary buffer for stringifying revision ids
  char ridBuffer[21];

  auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    TRI_voc_rid_t docRev;
    if (!RocksDBValue::revisionId(rocksValue, docRev)) {
      // for collections that do not have the revisionId in the value
      auto documentId = RocksDBValue::documentId(rocksValue); // we want probably to do this instead
      if (false == _collection->logical.readDocumentWithCallback(_trx.get(), documentId, [&docRev](LocalDocumentId const&, VPackSlice doc) {
        docRev = TRI_ExtractRevisionId(doc);
      })) {
        TRI_ASSERT(false);
        return true;
      }
    }

    StringRef docKey(RocksDBKey::primaryKey(rocksKey));
    b.openArray(true);
    b.add(velocypack::ValuePair(docKey.data(), docKey.size(), velocypack::ValueType::String));
    b.add(TRI_RidToValuePair(docRev, &ridBuffer[0]));
    b.close();

    return true;
  };

  b.openArray(true);
  // chunkSize is going to be ignored here
  try {
    _collection->hasMore = primary->next(cb, chunkSize);
    _lastIteratorOffset++;
  } catch (std::exception const& ex) {
    return rv.reset(TRI_ERROR_INTERNAL, ex.what());
  }
  b.close();

  return rv;
}

/// dump keys and document for incremental sync
arangodb::Result RocksDBReplicationContext::dumpDocuments(
    VPackBuilder& b, size_t chunk, size_t chunkSize, size_t offsetInChunk,
    size_t maxChunkSize, std::string const& lowKey, VPackSlice const& ids) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_collection->iter) {
    return rv.reset(
        TRI_ERROR_BAD_PARAMETER,
        "the replication context iterator has not been initialized");
  }
  _collection->setSorted(true, _trx.get());
  TRI_ASSERT(_collection->iter);
  TRI_ASSERT(_collection->sorted());
  //RocksDBSortedAllIterator* primary = static_cast<RocksDBSortedAllIterator*>(_collection->iter.get());
  auto primary = _collection->iter.get();

  // Position the iterator must be reset to the beginning
  // after calls to dumpKeys moved it forwards
  if (chunk != 0 &&
      ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER,
                    "It seems that your chunk / chunkSize combination is not "
                    "valid - overflow");
  }
  size_t from = chunk * chunkSize;

  if (from != _lastIteratorOffset) {
    if (!lowKey.empty()) {
      RocksDBKeyLeaser val(_trx.get());
      val->constructPrimaryIndexValue(primary->bounds().objectId(), StringRef(lowKey));
      primary->seek(val->string());
      _lastIteratorOffset = from;
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !_collection->hasMore || from < _lastIteratorOffset) {
        _collection->iter->reset();
        _lastIteratorOffset = 0;
      }
      if (from > _lastIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - _lastIteratorOffset;
        uint64_t to = 0;  // = (chunk + 1) * chunkSize;
        _collection->iter->skip(diff, to);
        _lastIteratorOffset += to;
        TRI_ASSERT(to == diff);
      }

      if (_lastIteratorOffset != from) {
        return rv.reset(
            TRI_ERROR_BAD_PARAMETER,
            "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }

  auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    auto documentId = RocksDBValue::documentId(rocksValue);
    bool ok = _collection->logical.readDocumentWithCallback(_trx.get(), documentId, [&b](LocalDocumentId const&, VPackSlice doc) {
      TRI_ASSERT(doc.isObject());
      b.add(doc);
    });
    if (!ok) {
      // TODO: do something here?
      return true;
    }
    return true;
  };

  auto buffer = b.buffer();
  bool hasMore = true;
  b.openArray();
  size_t oldPos = from;
  size_t offset = 0;

  for (auto const& it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      return Result(TRI_ERROR_BAD_PARAMETER);
    }

    if (!hasMore) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "Not enough data";
      b.close();
      return Result(TRI_ERROR_FAILED);
    }

    size_t newPos = from + it.getNumber<size_t>();
    if (newPos > oldPos) {
      uint64_t ignore = 0;
      primary->skip(newPos - oldPos, ignore);
      TRI_ASSERT(ignore == newPos - oldPos);
      _lastIteratorOffset += ignore;
    }

    bool full = false;
    if (offset < offsetInChunk) {
      // skip over the initial few documents
      hasMore = _collection->iter->next(
          [&b](rocksdb::Slice const&, rocksdb::Slice const&) {
            b.add(VPackValue(VPackValueType::Null));
            return true;
          },
          1);
    } else {
      hasMore = _collection->iter->next(cb, 1);
      if (buffer->byteSize() > maxChunkSize) {
        // result is big enough so that we abort prematurely
        full = true;
      }
    }

    _lastIteratorOffset++;
    oldPos = newPos + 1;
    ++offset;
    if (full) {
      break;
    }
  }
  b.close();
  _collection->hasMore = hasMore;

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

void RocksDBReplicationContext::deleted() {
  MUTEX_LOCKER(locker, _contextLock);
  _isDeleted = true;
}

bool RocksDBReplicationContext::isUsed() const {
  MUTEX_LOCKER(locker, _contextLock);
  return (_users > 0);
}

bool RocksDBReplicationContext::moreForDump(std::string const& collectionIdentifier) {
  MUTEX_LOCKER(locker, _contextLock);
  bool hasMore = false;
  TRI_voc_cid_t id{::normalizeIdentifier(*_trx, collectionIdentifier)};
  if (0 < id) {
    CollectionIterator* collection = getCollectionIterator(id, /*sorted*/false);
    if (collection) {
      hasMore = collection->hasMore;
      collection->release();
    }
  }
  return hasMore;
}

bool RocksDBReplicationContext::use(double ttl, bool exclusive) {
  MUTEX_LOCKER(locker, _contextLock);
  TRI_ASSERT(!_isDeleted);

  if (_exclusive || (exclusive && _users > 0)) {
    // can't get lock
    return false;
  }

  ++_users;
  _exclusive = exclusive;
  ttl = std::max(std::max(_ttl, ttl), replutils::BatchInfo::DefaultTimeout);
  _expires = TRI_microtime() + ttl;

  if (_serverId != 0) {
    _vocbase->updateReplicationClient(_serverId, ttl);
  }
  return true;
}

void RocksDBReplicationContext::release() {
  MUTEX_LOCKER(locker, _contextLock);
  TRI_ASSERT(_users > 0);
  double ttl = std::max(_ttl, replutils::BatchInfo::DefaultTimeout);
  _expires = TRI_microtime() + ttl;
  --_users;
  if (0 == _users) {
    _exclusive = false;
  }
  if (_serverId != 0) {
    double ttl;
    if (_ttl > 0.0) {
      // use TTL as configured
      ttl = _ttl;
    } else {
      // none configuration. use default
      ttl = replutils::BatchInfo::DefaultTimeout;
    }
    _vocbase->updateReplicationClient(_serverId, ttl);
  }
}

void RocksDBReplicationContext::releaseDumpingResources() {
  if (_trx != nullptr) {
    _trx->abort();
    _trx.reset();
  }
  if (_collection) {
    _collection->release();
    _collection = nullptr;
  }
  _iterators.clear();
  _guard.reset();
}

RocksDBReplicationContext::CollectionIterator::CollectionIterator(
    LogicalCollection& collection, transaction::Methods& trx,
    bool sorted)
    : logical{collection},
      iter{nullptr},
      currentTick{1},
      isUsed{false},
      hasMore{true},
      customTypeHandler{},
      vpackOptions{Options::Defaults},
      _sortedIterator{!sorted} // this makes sure that setSorted is not a noOp
{
  // we are getting into trouble during the dumping of "_users"
  // this workaround avoids the auth check in addCollectionAtRuntime
  ExecContext const* old = ExecContext::CURRENT;
  if (old != nullptr && old->systemAuthLevel() == auth::Level::RW) {
    ExecContext::CURRENT = nullptr;
  }
  TRI_DEFER(ExecContext::CURRENT = old);

  trx.addCollectionAtRuntime(collection.name());

  customTypeHandler = trx.transactionContextPtr()->orderCustomTypeHandler();
  vpackOptions.customTypeHandler = customTypeHandler.get();
  setSorted(sorted, &trx);
}

void RocksDBReplicationContext::CollectionIterator::setSorted(bool sorted,
                                                  transaction::Methods* trx) {
  if (_sortedIterator != sorted) {
    iter.reset();
    if (sorted) {
      auto iterator = createPrimaryIndexIterator(trx, &logical);
      iter = std::make_unique<RocksDBGenericIterator>(std::move(iterator)); //move to heap
    } else {
      auto iterator = createDocumentIterator(trx, &logical);
      iter = std::make_unique<RocksDBGenericIterator>(std::move(iterator)); //move to heap
    }
    currentTick = 1;
    hasMore = true;
    _sortedIterator = sorted;
  }
}

void RocksDBReplicationContext::CollectionIterator::release() {
  TRI_ASSERT(isUsed.load());
  isUsed.store(false);
}

RocksDBReplicationContext::CollectionIterator*
RocksDBReplicationContext::getCollectionIterator(TRI_voc_cid_t cid, bool sorted) {
  _contextLock.assertLockedByCurrentThread();

  CollectionIterator* collection{nullptr};
  // check if iterator already exists
  auto it = _iterators.find(cid);

  if (_iterators.end() != it) {
    // exists, check if used
    if (!it->second->isUsed.load()) {
      // unused, select it
      collection = it->second.get();
    }
  } else {
    // try to create one
    std::shared_ptr<LogicalCollection> logical{
      _trx->vocbase().lookupCollection(cid)
    };

    if (nullptr != logical) {
      TRI_ASSERT(nullptr != logical);
      TRI_ASSERT(nullptr != _trx);
      auto result = _iterators.emplace(
          cid, std::make_unique<CollectionIterator>(*logical, *_trx, sorted));

      if (result.second) {
        collection = result.first->second.get();

        if (nullptr == collection->iter) {
          collection = nullptr;
          _iterators.erase(cid);
        }
      }
    }
  }

  if (collection) {
    collection->isUsed.store(true);
    collection->setSorted(sorted, _trx.get());
  }

  return collection;
}
