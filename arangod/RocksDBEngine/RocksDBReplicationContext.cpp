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
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "Logger/Logger.h"
#include "Replication/common-defines.h"
#include "Replication/InitialSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBIterators.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "RocksDBEngine/RocksDBMethods.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "Utils/DatabaseGuard.h"
#include "Utils/ExecContext.h"
#include "VocBase/LocalDocumentId.h"
#include "VocBase/ticks.h"
#include "VocBase/vocbase.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

RocksDBReplicationContext::RocksDBReplicationContext(TRI_vocbase_t* vocbase, double ttl, TRI_server_id_t serverId)
    : _vocbase(vocbase),
      _serverId(serverId),
      _id(TRI_NewTickServer()),
      _lastTick(0),
      _currentTick(0),
      _trx(),
      _collection(nullptr),
      _lastIteratorOffset(0),
      _mdr(),
      _customTypeHandler(),
      _vpackOptions(Options::Defaults),
      _ttl(ttl),
      _expires(TRI_microtime() + _ttl),
      _isDeleted(false),
      _isUsed(true),
      _hasMore(true) {}

RocksDBReplicationContext::~RocksDBReplicationContext() {
  releaseDumpingResources();
}

TRI_voc_tick_t RocksDBReplicationContext::id() const { return _id; }

uint64_t RocksDBReplicationContext::lastTick() const { return _lastTick; }

uint64_t RocksDBReplicationContext::count() const {
  TRI_ASSERT(_trx != nullptr);
  TRI_ASSERT(_collection != nullptr);
  RocksDBCollection* rcoll = toRocksDBCollection(_collection->getPhysical());
  return rcoll->numberDocuments(_trx.get());
}

// creates new transaction/snapshot
void RocksDBReplicationContext::bind(TRI_vocbase_t* vocbase) {
  if (!_trx || !_guard || (_guard->database() != vocbase)) {
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
    _trx.reset(new transaction::UserTransaction(ctx, {}, {}, {},
                                                transactionOptions));
    auto state = RocksDBTransactionState::toState(_trx.get());
    if (snap != nullptr) {
      state->donateSnapshot(snap);
      TRI_ASSERT(snap->GetSequenceNumber() == state->sequenceNumber());
    }
    Result res = _trx->begin();
    if (!res.ok()) {
      _guard.reset();
      THROW_ARANGO_EXCEPTION(res);
    }
    _customTypeHandler = ctx->orderCustomTypeHandler();
    _vpackOptions.customTypeHandler = _customTypeHandler.get();
    _lastTick = state->sequenceNumber();
  }
}

int RocksDBReplicationContext::bindCollection(
    TRI_vocbase_t* vocbase,
    std::string const& collectionIdentifier,
    bool sorted) {
  bind(vocbase);
  
  if ((_collection == nullptr) ||
      ((_collection->name() != collectionIdentifier) &&
       std::to_string(_collection->cid()) != collectionIdentifier &&
       _collection->globallyUniqueId() != collectionIdentifier)) {
    _collection = _trx->vocbase()->lookupCollection(collectionIdentifier);
    if (_collection == nullptr) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    
    // we are getting into trouble during the dumping of "_users"
    // this workaround avoids the auth check in addCollectionAtRuntime
    ExecContext const* old = ExecContext::CURRENT;
    if (old != nullptr && old->systemAuthLevel() == auth::Level::RW) {
      ExecContext::CURRENT = nullptr;
    }
    TRI_DEFER(ExecContext::CURRENT = old);

    _trx->addCollectionAtRuntime(_collection->name());
    if (sorted) {
      auto iterator = createPrimaryIndexIterator(_trx.get(), _collection);
      _iter = std::make_unique<RocksDBGenericIterator>(std::move(iterator)); //move to heap
    } else {
      auto iterator = createDocumentIterator(_trx.get(), _collection);
      _iter = std::make_unique<RocksDBGenericIterator>(std::move(iterator)); //move to heap
    }
    _currentTick = 1;
    _hasMore = true;
  }
  return TRI_ERROR_NO_ERROR;
}

// returns inventory
std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>>
RocksDBReplicationContext::getInventory(TRI_vocbase_t* vocbase,
                                        bool includeSystem,
                                        bool global) {
  TRI_ASSERT(vocbase != nullptr);
  if (!_trx) {
    return std::make_pair(
        RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick),
        std::shared_ptr<VPackBuilder>(nullptr));
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

  auto tick = TRI_CurrentTickServer();

  if (global) {
    // global inventory
    auto builder = std::make_shared<VPackBuilder>();
    DatabaseFeature::DATABASE->inventory(*builder.get(), tick, nameFilter);
    return std::make_pair(RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick),
                          builder);
  } else {
    // database-specific inventory
    auto builder = std::make_shared<VPackBuilder>();
    vocbase->inventory(*builder.get(), tick, nameFilter);

    return std::make_pair(RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick),
                          builder);
  }
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationResult RocksDBReplicationContext::dump(
    TRI_vocbase_t* vocbase, std::string const& collectionName,
    basics::StringBuffer& buff, uint64_t chunkSize) {
  TRI_ASSERT(vocbase != nullptr);
  if (!_trx) {
    return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
  }
  int res = bindCollection(vocbase, collectionName, false);
  if (res != TRI_ERROR_NO_ERROR) {
    return RocksDBReplicationResult(res, _lastTick);
  }
  if (!_iter) {
    return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, "the replication context iterator has not been initialized", _lastTick);
  }

  // reserve chunkSize plus some overhead for JSON packaging
  buff.reserve(std::min(size_t(chunkSize * 1.10), size_t(10 * 1000 * 1000)));

  arangodb::basics::VPackStringBufferAdapter adapter(buff.stringBuffer());

  auto cb = [&adapter, &buff, this](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    buff.appendText("{\"type\":");
    buff.appendInteger(REPLICATION_MARKER_DOCUMENT); // set type
    buff.appendText(",\"data\":");

    // printing the data, note: we need the CustomTypeHandler here
    VPackDumper dumper(&adapter, &_vpackOptions);
    dumper.dump(velocypack::Slice(rocksValue.data()));
    buff.appendText("}\n", 2);
  };
  
  TRI_ASSERT(_iter);

  while (_hasMore && buff.length() < chunkSize) {
    try {
      _hasMore = _iter->next(cb, 1);  // TODO: adjust limit?
    } catch (std::exception const&) {
      _hasMore = false;
      return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    } catch (RocksDBReplicationResult const& ex) {
      _hasMore = false;
      return ex;
    }
  }

  if (_hasMore) {
    _currentTick++;
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _currentTick);
}

arangodb::Result RocksDBReplicationContext::dumpKeyChunks(VPackBuilder& b,
                                                          uint64_t chunkSize) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_iter) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "the replication context iterator has not been initialized");
  }

  std::string lowKey;
  std::string highKey;  // needs to be a string (not ref) as the rocksdb slice will not be valid outside the callback
  VPackBuilder builder;
  uint64_t hash = 0x012345678;

  auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    highKey = RocksDBKey::primaryKey(rocksKey).toString();

    TRI_voc_rid_t docRev;
    if(!RocksDBValue::revisionId(rocksValue, docRev)){
      // for collections that do not have the revisionId in the value
      auto documentId = RocksDBValue::documentId(rocksValue); // we want probably to do this instead
      if(_collection->readDocument(_trx.get(), documentId, _mdr) == false) {
        TRI_ASSERT(false);
        return;
      }
      VPackSlice doc(_mdr.vpack());
      docRev = TRI_ExtractRevisionId(doc);
    }

    // set type
    if (lowKey.empty()) {
      lowKey = highKey;
    }

    // we can get away with the fast hash function here, as key values are
    // restricted to strings
    builder.clear();
    builder.add(VPackValue(highKey));
    hash ^= builder.slice().hashString();
    builder.clear();
    builder.add(VPackValue(TRI_RidToString(docRev)));
    hash ^= builder.slice().hash();
  }; //cb

  b.openArray();
  while (_hasMore) {
    try {
      _hasMore = _iter->next(cb, chunkSize);

      if (lowKey.empty()) {
        // if lowKey is empty, no new documents were found
        break;
      }
      b.add(VPackValue(VPackValueType::Object));
      b.add("low", VPackValue(lowKey));
      b.add("high", VPackValue(highKey));
      b.add("hash", VPackValue(std::to_string(hash)));
      b.close();
      lowKey.clear();  // reset string
      hash = 0x012345678;   // the next block ought to start with a clean sheet
    } catch (std::exception const&) {
      return rv.reset(TRI_ERROR_INTERNAL);
    }
  }

  b.close();
  // we will not call this method twice
  _iter->reset();
  _lastIteratorOffset = 0;

  return rv;
}

/// dump all keys from collection
arangodb::Result RocksDBReplicationContext::dumpKeys(
    VPackBuilder& b, size_t chunk, size_t chunkSize,
    std::string const& lowKey) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_iter) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "the replication context iterator has not been initialized");
  }

  TRI_ASSERT(_iter);
  auto primary = _iter.get();

  // Position the iterator correctly
  if (chunk != 0 && ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "It seems that your chunk / chunkSize combination is not valid - overflow");
  }

  size_t from = chunk * chunkSize;

  if (from != _lastIteratorOffset) {
    if (!lowKey.empty()) {
      RocksDBKeyLeaser val(_trx.get());
      val->constructPrimaryIndexValue(primary->bounds().objectId(), StringRef(lowKey));
      primary->seek(val->string());
      _lastIteratorOffset = from;
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !_hasMore || from < _lastIteratorOffset) {
        _iter->reset();
        _lastIteratorOffset = 0;
      }

      if (from > _lastIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - _lastIteratorOffset;
        uint64_t to = 0;  // = (chunk + 1) * chunkSize;
        _iter->skip(diff, to);
        _lastIteratorOffset += to;
      }

      //TRI_ASSERT(_lastIteratorOffset == from);
      if(_lastIteratorOffset != from){
        return rv.reset(TRI_ERROR_BAD_PARAMETER, "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }

 auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    TRI_voc_rid_t docRev;
    if(!RocksDBValue::revisionId(rocksValue, docRev)){
      // for collections that do not have the revisionId in the value
      auto documentId = RocksDBValue::documentId(rocksValue); // we want probably to do this instead
      if(_collection->readDocument(_trx.get(), documentId, _mdr) == false) {
        TRI_ASSERT(false);
        return;
      }
      VPackSlice doc(_mdr.vpack());
      docRev = TRI_ExtractRevisionId(doc);
    }
 
    StringRef docKey(RocksDBKey::primaryKey(rocksKey));
    b.openArray();
    b.add(velocypack::ValuePair(docKey.data(), docKey.size(), velocypack::ValueType::String));
    b.add(VPackValue(TRI_RidToString(docRev)));
    b.close();
  };

  b.openArray();
  // chunkSize is going to be ignored here
  try {
    _hasMore = primary->next(cb, chunkSize);
    _lastIteratorOffset++;
  } catch (std::exception const&) {
    return rv.reset(TRI_ERROR_INTERNAL);
  }
  b.close();

  return rv;
}

/// dump keys and document
arangodb::Result RocksDBReplicationContext::dumpDocuments(
    VPackBuilder& b, size_t chunk, size_t chunkSize, size_t offsetInChunk,
    size_t maxChunkSize, std::string const& lowKey, VPackSlice const& ids) {
  TRI_ASSERT(_trx);

  Result rv;
  if (!_iter) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "the replication context iterator has not been initialized");
  }

  TRI_ASSERT(_iter);
  auto primary = _iter.get();

  // Position the iterator must be reset to the beginning
  // after calls to dumpKeys moved it forwards
  if (chunk != 0 && ((std::numeric_limits<std::size_t>::max() / chunk) < chunkSize)) {
    return rv.reset(TRI_ERROR_BAD_PARAMETER, "It seems that your chunk / chunkSize combination is not valid - overflow");
  }
  size_t from = chunk * chunkSize;

  if (from != _lastIteratorOffset) {
    if (!lowKey.empty()) {
      RocksDBKeyLeaser val(_trx.get());
      val->constructPrimaryIndexValue(primary->bounds().objectId(), StringRef(lowKey));
      primary->seek(val->string());
      _lastIteratorOffset = from;
    } else {  // no low key supplied, we can not use seek
      if (from == 0 || !_hasMore || from < _lastIteratorOffset) {
        _iter->reset();
        _lastIteratorOffset = 0;
      }
      if (from > _lastIteratorOffset) {
        TRI_ASSERT(from >= chunkSize);
        uint64_t diff = from - _lastIteratorOffset;
        uint64_t to = 0;  // = (chunk + 1) * chunkSize;
        _iter->skip(diff, to);
        _lastIteratorOffset += to;
        TRI_ASSERT(to == diff);
      }

      if(_lastIteratorOffset != from){
        return rv.reset(TRI_ERROR_BAD_PARAMETER, "The parameters you provided lead to an invalid iterator offset.");
      }
    }
  }

  auto cb = [&](rocksdb::Slice const& rocksKey, rocksdb::Slice const& rocksValue) {
    auto documentId = RocksDBValue::documentId(rocksValue);
    bool ok = _collection->readDocument(_trx.get(), documentId, _mdr);
    if (!ok) {
      // TODO: do something here?
      return;
    }
    VPackSlice current(_mdr.vpack());
    TRI_ASSERT(current.isObject());
    b.add(current);
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
      hasMore = _iter->next(
          [&b](rocksdb::Slice const&, rocksdb::Slice const&) {
        b.add(VPackValue(VPackValueType::Null));
      }, 1);
    } else {
      hasMore = _iter->next(cb, 1);
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
  _hasMore = hasMore;

  return Result();
}

double RocksDBReplicationContext::expires() const { return _expires; }

bool RocksDBReplicationContext::isDeleted() const { return _isDeleted; }

void RocksDBReplicationContext::deleted() { _isDeleted = true; }

bool RocksDBReplicationContext::isUsed() const { return _isUsed; }
bool RocksDBReplicationContext::more() const { return _hasMore; }

void RocksDBReplicationContext::use(double ttl) {
  TRI_ASSERT(!_isDeleted);
  TRI_ASSERT(!_isUsed);

  _isUsed = true;
  ttl = std::max(std::max(_ttl, ttl), 300.0);
  _expires = TRI_microtime() + ttl;
  if (_serverId != 0) {
    _vocbase->updateReplicationClient(_serverId, ttl);
  }
}

void RocksDBReplicationContext::release() {
  TRI_ASSERT(_isUsed);
  double ttl = std::max(_ttl, 300.0);
  _expires = TRI_microtime() + ttl;
  _isUsed = false;
  if (_serverId != 0) {
    double ttl;
    if (_ttl > 0.0) {
      // use TTL as configured
      ttl = _ttl;
    } else {
      // none configuration. use default
      ttl = InitialSyncer::defaultBatchTimeout;
    }
    _vocbase->updateReplicationClient(_serverId, ttl);
  }
}

void RocksDBReplicationContext::releaseDumpingResources() {
  _iter.reset();
  if (_trx != nullptr) {
    _trx->abort();
    _trx.reset();
  }
  _collection = nullptr;
  _guard.reset();
}
