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
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBTransactionState.h"
#include "Transaction/StandaloneContext.h"
#include "Transaction/UserTransaction.h"
#include "VocBase/replication-common.h"
#include "VocBase/ticks.h"
#include "Basics/StringBuffer.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

/// WAL parser, no locking required here, because we have been locked from the
/// outside
class WBReader : public rocksdb::WriteBatch::Handler {
 public:
  explicit WBReader(TRI_vocbase_t* vocbase, uint64_t from, size_t& limit,
                    bool includeSystem, VPackBuilder& builder)
      : _vocbase(vocbase),
        _from(from),
        _limit(limit),
        _includeSystem(includeSystem),
        _builder(builder) {}

  void Put(rocksdb::Slice const& key, rocksdb::Slice const& value) override {
    if (shouldHandleKey(key)) {
      int res = TRI_ERROR_NO_ERROR;
      _builder.openObject();
      switch (RocksDBKey::type(key)) {
        case RocksDBEntryType::Collection: {
          _builder.add(
              "type",
              VPackValue(static_cast<uint64_t>(REPLICATION_COLLECTION_CREATE)));
        }
        case RocksDBEntryType::Document: {
          _builder.add(
              "type",
              VPackValue(static_cast<uint64_t>(REPLICATION_MARKER_DOCUMENT)));
          // TODO: add transaction id?
          break;
        }
        default:
          break;  // shouldn't get here?
      }

      auto containers = getContainerIds(key);
      _builder.add("database", VPackValue(containers.first));
      _builder.add("cid", VPackValue(containers.second));
      _builder.add("data", RocksDBValue::data(value));

      _builder.close();

      if (res == TRI_ERROR_NO_ERROR) {
        _limit--;
      }
    }
  }

  void Delete(rocksdb::Slice const& key) override { handleDeletion(key); }

  void SingleDelete(rocksdb::Slice const& key) override { handleDeletion(key); }

 private:
  bool shouldHandleKey(rocksdb::Slice const& key) {
    if (_limit == 0) {
      return false;
    }

    switch (RocksDBKey::type(key)) {
      case RocksDBEntryType::Collection:
      case RocksDBEntryType::Document: {
        return fromEligibleCollection(key);
      }
      case RocksDBEntryType::View:  // should handle these eventually?
      default:
        return false;
    }
  }

  void handleDeletion(rocksdb::Slice const& key) {
    if (shouldHandleKey(key)) {
      int res = TRI_ERROR_NO_ERROR;
      _builder.openObject();
      switch (RocksDBKey::type(key)) {
        case RocksDBEntryType::Collection: {
          _builder.add(
              "type",
              VPackValue(static_cast<uint64_t>(REPLICATION_COLLECTION_DROP)));
          auto containers = getContainerIds(key);
          _builder.add("database", VPackValue(containers.first));
          _builder.add("cid", VPackValue(containers.second));
          break;
        }
        case RocksDBEntryType::Document: {
          uint64_t revisionId = RocksDBKey::revisionId(key);
          _builder.add(
              "type",
              VPackValue(static_cast<uint64_t>(REPLICATION_MARKER_REMOVE)));
          // TODO: add transaction id?
          auto containers = getContainerIds(key);
          _builder.add("database", VPackValue(containers.first));
          _builder.add("cid", VPackValue(containers.second));
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add(StaticStrings::RevString,
                       VPackValue(std::to_string(revisionId)));
          _builder.close();
          break;
        }
        default:
          break;  // shouldn't get here?
      }
      _builder.close();
      if (res == TRI_ERROR_NO_ERROR) {
        _limit--;
      }
    }
  }

  std::pair<TRI_voc_tick_t, TRI_voc_cid_t> getContainerIds(
      rocksdb::Slice const& key) {
    uint64_t objectId = RocksDBKey::collectionId(key);
    return mapObjectToCollection(objectId);
  }

  bool fromEligibleCollection(rocksdb::Slice const& key) {
    auto mapping = getContainerIds(key);
    if (mapping.first == _vocbase->id()) {
      std::string const collectionName =
          _vocbase->collectionName(mapping.second);

      if (collectionName.size() == 0) {
        return false;
      }

      if (!_includeSystem && collectionName[0] == '_') {
        return false;
      }

      return true;
    }
    return false;
  }

 private:
  TRI_vocbase_t* _vocbase;
  uint64_t _from;
  size_t& _limit;
  bool _includeSystem;
  VPackBuilder& _builder;
};

RocksDBReplicationResult::RocksDBReplicationResult(int errorNumber,
                                                   uint64_t maxTick)
    : Result(errorNumber), _maxTick(maxTick) {}

uint64_t RocksDBReplicationResult::maxTick() const { return _maxTick; }

RocksDBReplicationContext::RocksDBReplicationContext()
    : _id(TRI_NewTickServer()),
      _lastTick(0),
      _trx(),
      _collection(nullptr),
      _iter(),
      _hasMore(true) {}

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
    _iter = _collection->getAllIterator(_trx.get(), &_mdr, false); //_mdr is not used nor updated
  }

  bool const isEdge = _collection->type() == TRI_COL_TYPE_EDGE;
  VPackBuilder builder;


  auto cb = [this, isEdge, &buff, &builder](DocumentIdentifierToken const& token){
    builder.clear();

    //set type
    int type = 2300; // documents
    builder.openObject();
    if (isEdge) {
      type = 2301; // edge documents
    }
    builder.add("type", VPackValue(type));

    //set data
    int res = _collection->readDocument(_trx.get(), token, _mdr);
    if (res != TRI_ERROR_NO_ERROR){
      //fail
    }
    builder.add("data", VPackSlice(_mdr.vpack()));
    builder.close();
    buff.appendText(builder.toJson());
  };

  while(_hasMore && true /*sizelimit*/) {
    try {
      _hasMore = _iter->next(cb, limit);
    }
    catch (std::exception const& e) {
        return RocksDBReplicationResult(TRI_ERROR_INTERNAL, _lastTick);
    }
  }

  return RocksDBReplicationResult(TRI_ERROR_NO_ERROR, _lastTick);
}

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database
RocksDBReplicationResult RocksDBReplicationContext::tail(
    TRI_vocbase_t* vocbase, uint64_t from, size_t limit, bool includeSystem,
    VPackBuilder& builder) {
  releaseDumpingResources();
  std::unique_ptr<WBReader> handler(
      new WBReader(vocbase, from, limit, includeSystem, builder));
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = static_cast<rocksdb::DB*>(globalRocksDB())
                          ->GetUpdatesSince(from, &iterator);
  if (!s.ok()) {  // TODO do something?
    auto converted = convertStatus(s);
    return {converted.errorNumber(), _lastTick};
  }

  while (iterator->Valid() && limit > 0) {
    s = iterator->status();
    if (s.ok()) {
      rocksdb::BatchResult batch = iterator->GetBatch();
      _lastTick = batch.sequence;
      s = batch.writeBatchPtr->Iterate(handler.get());
    }
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "Error during WAL scan";
      LOG_TOPIC(ERR, Logger::ENGINES) << iterator->status().getState();
      auto converted = convertStatus(s);
      return {converted.errorNumber(), _lastTick};
    }

    iterator->Next();
  }

  return {TRI_ERROR_NO_ERROR, _lastTick};
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
