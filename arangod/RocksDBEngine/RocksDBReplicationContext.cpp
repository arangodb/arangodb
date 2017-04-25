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

#include "RocksDBEngine/RocksDBReplicationContext.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringBuffer.h"
#include "Basics/StringRef.h"
#include "Basics/VPackStringBufferAdapter.h"
#include "RocksDBEngine/RocksDBCollection.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBPrimaryIndex.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "VocBase/replication-common.h"
#include "VocBase/ticks.h"

#include <velocypack/Dumper.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

double const RocksDBReplicationContext::DefaultTTL = 30 * 60.0;

RocksDBReplicationContext::RocksDBReplicationContext()
    : _id(TRI_NewTickServer()),
      _lastTick(0),
      _trx(),
      _collection(nullptr),
      _iter(),
      _mdr(),
      _customTypeHandler(),
      _vpackOptions(Options::Defaults),
      _expires(TRI_microtime() + DefaultTTL),
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
  RocksDBCollection* rcoll =
      RocksDBCollection::toRocksDBCollection(_collection->getPhysical());
  return rcoll->numberDocuments(_trx.get());
}

// creates new transaction/snapshot
void RocksDBReplicationContext::bind(TRI_vocbase_t* vocbase) {
  releaseDumpingResources();
  _trx = createTransaction(vocbase);
}

int RocksDBReplicationContext::bindCollection(
    std::string const& collectionName) {
  if ((_collection == nullptr) || _collection->name() != collectionName) {
    _collection = _trx->vocbase()->lookupCollection(collectionName);

    if (_collection == nullptr) {
      return TRI_ERROR_BAD_PARAMETER;
    }
    _trx->addCollectionAtRuntime(collectionName);
    _iter = _collection->getAllIterator(_trx.get(), &_mdr,
                                        false);  //_mdr is not used nor updated
    _hasMore = true;
  }
  return TRI_ERROR_NO_ERROR;
}

// returns inventory
std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>>
RocksDBReplicationContext::getInventory(TRI_vocbase_t* vocbase,
                                        bool includeSystem) {
  TRI_ASSERT(vocbase != nullptr);
  if (_trx.get() == nullptr) {
    return std::make_pair(
        RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick),
        std::shared_ptr<VPackBuilder>(nullptr));
  }

  auto tick = TRI_CurrentTickServer();
  _lastTick = toRocksTransactionState(_trx.get())->sequenceNumber();
  std::shared_ptr<VPackBuilder> inventory = vocbase->inventory(
      tick, filterCollection, &includeSystem, true, sortCollections);

  return std::make_pair(RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick),
                        inventory);
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationResult RocksDBReplicationContext::dump(
    TRI_vocbase_t* vocbase, std::string const& collectionName,
    basics::StringBuffer& buff, uint64_t chunkSize) {
  TRI_ASSERT(vocbase != nullptr);
  if (_trx.get() == nullptr) {
    return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
  }
  int res = bindCollection(collectionName);
  if (res != TRI_ERROR_NO_ERROR) {
    return RocksDBReplicationResult(res, _lastTick);
  }

  // set type
  int type = 2300;  // documents
  if (_collection->type() == TRI_COL_TYPE_EDGE) {
    type = 2301;  // edge documents
  }

  arangodb::basics::VPackStringBufferAdapter adapter(buff.stringBuffer());

  VPackBuilder builder(&_vpackOptions);

  uint64_t size = 0;
  auto cb = [this, &type, &buff, &adapter, &size,
             &builder](DocumentIdentifierToken const& token) {
    builder.clear();

    builder.openObject();
    // set type
    builder.add("type", VPackValue(type));

    // set data
    bool ok = _collection->readDocument(_trx.get(), token, _mdr);

    if (!ok) {
      LOG_TOPIC(ERR, Logger::REPLICATION)
          << "could not get document with token: " << token._data;
      throw RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    }

    builder.add(VPackValue("data"));
    _mdr.addToBuilder(builder, false);
    builder.close();

    VPackDumper dumper(
        &adapter,
        &_vpackOptions);  // note: we need the CustomTypeHandler here
    VPackSlice slice = builder.slice();
    dumper.dump(slice);
    buff.appendChar('\n');
    size += (slice.byteSize() + 1);
  };

  while (_hasMore && (size < chunkSize)) {
    try {
      _hasMore = _iter->next(cb, 10);  // TODO: adjust limit?
    } catch (std::exception const& ex) {
      return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    } catch (RocksDBReplicationResult const& ex) {
      return ex;
    }
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick);
}

arangodb::Result RocksDBReplicationContext::dumpKeyChunks(VPackBuilder& b,
                                                          uint64_t chunkSize) {
  TRI_ASSERT(_trx);
  TRI_ASSERT(_iter);

  RocksDBAllIndexIterator* primary =
      dynamic_cast<RocksDBAllIndexIterator*>(_iter.get());

  std::string lowKey;
  VPackSlice highKey;  // FIXME: no good keeping this

  uint64_t hash = 0x012345678;
  auto cb = [&](DocumentIdentifierToken const& token) {
    bool ok = _collection->readDocument(_trx.get(), token, _mdr);
    if (!ok) {
      // TODO: do something here?
      return;
    }

    // current document
    VPackSlice current(_mdr.vpack());
    highKey = current.get(StaticStrings::KeyString);
    // set type
    if (lowKey.empty()) {
      lowKey = highKey.copyString();
    }

    // we can get away with the fast hash function here, as key values are
    // restricted to strings
    hash ^= transaction::helpers::extractKeyFromDocument(current).hashString();
    hash ^= transaction::helpers::extractRevSliceFromDocument(current).hash();
  };

  b.openArray();
  while (_hasMore && true /*sizelimit*/) {
    try {
      _hasMore = primary->next(cb, chunkSize);

      b.add(VPackValue(VPackValueType::Object));
      b.add("low", VPackValue(lowKey));
      b.add("high", VPackValue(highKey.copyString()));
      b.add("hash", VPackValue(std::to_string(hash)));
      b.close();
      lowKey = "";
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL);
    }
  }
  b.close();

  return Result();
}

/// dump all keys from collection
arangodb::Result RocksDBReplicationContext::dumpKeys(VPackBuilder& b,
                                                     size_t chunk,
                                                     size_t chunkSize) {
  TRI_ASSERT(_trx);
  TRI_ASSERT(_iter);
  // Position the iterator correctly
  size_t from = chunk * chunkSize;
  if (from == 0) {
    _iter->reset();
    _lastChunkOffset = 0;
    _hasMore = true;
  } else if (from < _lastChunkOffset + chunkSize) {
    TRI_ASSERT(from >= chunkSize);
    uint64_t diff = from - chunkSize;
    uint64_t to;  // = (chunk + 1) * chunkSize;
    _iter->skip(diff, to);
    TRI_ASSERT(to == diff);
    _lastChunkOffset = from;
  } else if (from > _lastChunkOffset + chunkSize) {
    // no jumping back in time fix the intitial syncer if you see this
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "Trying to request a chunk the rocksdb "
        << "iterator already passed over";
    return Result(TRI_ERROR_INTERNAL);
  }

  RocksDBAllIndexIterator* primary =
      dynamic_cast<RocksDBAllIndexIterator*>(_iter.get());
  auto cb = [&](DocumentIdentifierToken const& token, StringRef const& key) {
    RocksDBToken const& rt = static_cast<RocksDBToken const&>(token);

    b.openArray();
    b.add(VPackValuePair(key.data(), key.size(), VPackValueType::String));
    b.add(VPackValue(std::to_string(rt.revisionId())));
    b.close();
  };

  b.openArray();
  // chunk is going to be ignored here
  while (_hasMore && true /*sizelimit*/) {
    try {
      _hasMore = primary->nextWithKey(cb, chunkSize);
    } catch (std::exception const& ex) {
      return Result(TRI_ERROR_INTERNAL);
    }
  }
  b.close();

  return Result();
}

/// dump keys and document
arangodb::Result RocksDBReplicationContext::dumpDocuments(
    VPackBuilder& b, size_t chunk, size_t chunkSize, VPackSlice const& ids) {
  TRI_ASSERT(_trx);
  TRI_ASSERT(_iter);
  // Position the iterator correctly
  size_t from = chunk * chunkSize;
  if (from == 0) {
    _iter->reset();
    _lastChunkOffset = 0;
    _hasMore = true;
  } else if (from < _lastChunkOffset + chunkSize) {
    TRI_ASSERT(from >= chunkSize);
    uint64_t diff = from - chunkSize;
    uint64_t to;  // = (chunk + 1) * chunkSize;
    _iter->skip(diff, to);
    TRI_ASSERT(to == diff);
    _lastChunkOffset = from;
  } else if (from > _lastChunkOffset + chunkSize) {
    // no jumping back in time fix the intitial syncer if you see this
    LOG_TOPIC(ERR, Logger::REPLICATION)
        << "Trying to request a chunk the rocksdb "
        << "iterator already passed over";
    return Result(TRI_ERROR_INTERNAL);
  }

  auto cb = [&](DocumentIdentifierToken const& token) {
    bool ok = _collection->readDocument(_trx.get(), token, _mdr);
    if (!ok) {
      // TODO: do something here?
      return;
    }
    VPackSlice current(_mdr.vpack());
    TRI_ASSERT(current.isObject());
    b.add(current);
  };

  b.openArray();
  bool hasMore = true;
  size_t oldPos = from;
  for (auto const& it : VPackArrayIterator(ids)) {
    if (!it.isNumber()) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
    }
    TRI_ASSERT(hasMore);

    size_t newPos = from + it.getNumber<size_t>();
    if (oldPos != from && newPos > oldPos + 1) {
      uint64_t ignore;
      _iter->skip(newPos - oldPos, ignore);
      TRI_ASSERT(ignore == newPos - oldPos);
    }
    hasMore = _iter->next(cb, 1);
  }
  b.close();

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
  _expires = TRI_microtime() + ttl;
}

void RocksDBReplicationContext::release() {
  TRI_ASSERT(_isUsed);
  _isUsed = false;
}

void RocksDBReplicationContext::releaseDumpingResources() {
  if (_trx.get() != nullptr) {
    _trx->abort();
    _trx.reset();
  }
  if (_iter.get() != nullptr) {
    _iter.reset();
  }
  _collection = nullptr;
}

std::unique_ptr<transaction::Methods>
RocksDBReplicationContext::createTransaction(TRI_vocbase_t* vocbase) {
  double lockTimeout = transaction::Methods::DefaultLockTimeout;
  std::shared_ptr<transaction::StandaloneContext> ctx =
      transaction::StandaloneContext::Create(vocbase);
  std::unique_ptr<transaction::Methods> trx(new transaction::UserTransaction(
      ctx, {}, {}, {}, lockTimeout, false, true));
  Result res = trx->begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
  _customTypeHandler = ctx->orderCustomTypeHandler();
  _vpackOptions.customTypeHandler = _customTypeHandler.get();

  return trx;
}

/// @brief filter a collection based on collection attributes
bool RocksDBReplicationContext::filterCollection(
    arangodb::LogicalCollection* collection, void* data) {
  bool includeSystem = *((bool*)data);

  std::string const collectionName(collection->name());

  if (!includeSystem && collectionName[0] == '_') {
    // exclude all system collections
    return false;
  }

  if (TRI_ExcludeCollectionReplication(collectionName.c_str(), includeSystem)) {
    // collection is excluded from replication
    return false;
  }

  // all other cases should be included
  return true;
}

bool RocksDBReplicationContext::sortCollections(
    arangodb::LogicalCollection const* l,
    arangodb::LogicalCollection const* r) {
  if (l->type() != r->type()) {
    return l->type() < r->type();
  }
  std::string const leftName = l->name();
  std::string const rightName = r->name();

  return strcasecmp(leftName.c_str(), rightName.c_str()) < 0;
}
