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
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "VocBase/replication-common.h"
#include "VocBase/ticks.h"

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
      _expires(TRI_microtime() + DefaultTTL),
      _isDeleted(false),
      _isUsed(true),
      _hasMore(true) {}

RocksDBReplicationContext::~RocksDBReplicationContext() {
  releaseDumpingResources();
}

TRI_voc_tick_t RocksDBReplicationContext::id() const { return _id; }

uint64_t RocksDBReplicationContext::lastTick() const { return _lastTick; }

// creates new transaction/snapshot, returns inventory
std::pair<RocksDBReplicationResult, std::shared_ptr<VPackBuilder>>
RocksDBReplicationContext::getInventory(TRI_vocbase_t* vocbase,
                                        bool includeSystem) {
  TRI_ASSERT(vocbase != nullptr);
  if (_trx.get() != nullptr) {
    return std::make_pair(
        RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick), nullptr);
  }

  _trx = createTransaction(vocbase);
  auto tick = TRI_CurrentTickServer();
  _lastTick = toRocksTransactionState(_trx.get())->sequenceNumber();
  std::shared_ptr<VPackBuilder> inventory = vocbase->inventory(
      tick, filterCollection, &includeSystem, true, sortCollections);

  return std::make_pair(
      RocksDBReplicationResult(TRI_ERROR_NOT_YET_IMPLEMENTED, _lastTick),
      inventory);
}

// iterates over at most 'limit' documents in the collection specified,
// creating a new iterator if one does not exist for this collection
RocksDBReplicationResult RocksDBReplicationContext::dump(
    TRI_vocbase_t* vocbase, std::string const& collectionName,
    basics::StringBuffer& buff, size_t limit) {
  TRI_ASSERT(vocbase != nullptr);
  if (_trx.get() == nullptr) {
    return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
  }

  if ((_collection == nullptr) || _collection->name() != collectionName) {
    _collection = vocbase->lookupCollection(collectionName);
    if (_collection == nullptr) {
      return RocksDBReplicationResult(TRI_ERROR_BAD_PARAMETER, _lastTick);
    }
    _iter = _collection->getAllIterator(_trx.get(), &_mdr,
                                        false);  //_mdr is not used nor updated
  }

  // set type
  int type = 2300;  // documents
  if (_collection->type() == TRI_COL_TYPE_EDGE) {
    type = 2301;  // edge documents
  }

  VPackBuilder builder;

  auto cb = [this, &type, &buff,
             &builder](DocumentIdentifierToken const& token) {
    builder.clear();

    builder.openObject();
    // set type
    builder.add("type", VPackValue(type));

    // set data
    int res = _collection->readDocument(_trx.get(), token, _mdr);
    if (res != TRI_ERROR_NO_ERROR) {
      // fail
    }

    builder.add(VPackValue("data"));
    _mdr.addToBuilder(builder, false);
    builder.close();
    buff.appendText(builder.toJson());
  };

  while (_hasMore && true /*sizelimit*/) {
    try {
      _hasMore = _iter->next(cb, limit);
    } catch (std::exception const& e) {
      return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    }
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick);
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
}

std::unique_ptr<transaction::Methods>
RocksDBReplicationContext::createTransaction(TRI_vocbase_t* vocbase) {
  double lockTimeout = transaction::Methods::DefaultLockTimeout;
  auto ctx = transaction::StandaloneContext::Create(vocbase);
  std::unique_ptr<transaction::Methods> trx(new transaction::UserTransaction(
      ctx, {}, {}, {}, lockTimeout, false, true));
  Result res = trx->begin();
  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }
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
