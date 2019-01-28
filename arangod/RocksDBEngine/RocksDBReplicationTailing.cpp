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

#include "RocksDBReplicationTailing.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Logger/Logger.h"
#include "Replication/common-defines.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "Utils/CollectionGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::rocksutils;

// define to INFO to see the WAL output
#define _LOG TRACE

namespace {
static std::string const emptyString;
}

/// an incomplete convert function, basically only use for DDL ops
TRI_replication_operation_e rocksutils::convertLogType(RocksDBLogType t) {
  switch (t) {
    case RocksDBLogType::DatabaseCreate:
      return REPLICATION_DATABASE_CREATE;
    case RocksDBLogType::DatabaseDrop:
      return REPLICATION_DATABASE_DROP;
    case RocksDBLogType::CollectionCreate:
      return REPLICATION_COLLECTION_CREATE;
    case RocksDBLogType::CollectionDrop:
      return REPLICATION_COLLECTION_DROP;
    case RocksDBLogType::CollectionRename:
      return REPLICATION_COLLECTION_RENAME;
    case RocksDBLogType::CollectionChange:
      return REPLICATION_COLLECTION_CHANGE;
    case RocksDBLogType::CollectionTruncate:
      return REPLICATION_COLLECTION_TRUNCATE;
    case RocksDBLogType::IndexCreate:
      return REPLICATION_INDEX_CREATE;
    case RocksDBLogType::IndexDrop:
      return REPLICATION_INDEX_DROP;
    case RocksDBLogType::ViewCreate:
      return REPLICATION_VIEW_CREATE;
    case RocksDBLogType::ViewDrop:
      return REPLICATION_VIEW_DROP;
    case RocksDBLogType::ViewChange:
      return REPLICATION_VIEW_CHANGE;
    case RocksDBLogType::BeginTransaction:
      return REPLICATION_TRANSACTION_START;
    case RocksDBLogType::CommitTransaction:
      return REPLICATION_TRANSACTION_COMMIT;

    default:
      TRI_ASSERT(false);
      return REPLICATION_INVALID;
  }
}

/// WAL parser
class WALParser final : public rocksdb::WriteBatch::Handler {
  // internal WAL parser states
  enum State : char {
    INVALID = 0,
    COLLECTION_CREATE,
    COLLECTION_DROP,
    COLLECTION_RENAME,
    COLLECTION_CHANGE,
    INDEX_CREATE,
    INDEX_DROP,
    VIEW_CREATE,
    VIEW_DROP,
    VIEW_CHANGE,
    VIEW_RENAME,
    TRANSACTION,
    SINGLE_PUT,
    SINGLE_REMOVE
  };

 public:
  WALParser(TRI_vocbase_t* vocbase, bool includeSystem,
            TRI_voc_cid_t collectionId, VPackBuilder& builder)
      : _definitionsCF(RocksDBColumnFamily::definitions()->GetID()),
        _documentsCF(RocksDBColumnFamily::documents()->GetID()),
        _primaryCF(RocksDBColumnFamily::primary()->GetID()),

        _vocbase(vocbase),
        _includeSystem(includeSystem),
        _onlyCollectionId(collectionId),
        _builder(builder),
        _startSequence(0),
        _currentSequence(0),
        _lastEmittedTick(0) {}

  void LogData(rocksdb::Slice const& blob) override {
    RocksDBLogType type = RocksDBLogValue::type(blob);

    LOG_TOPIC(_LOG, Logger::REPLICATION) << "[LOG] " << rocksDBLogTypeName(type);
    switch (type) {
      case RocksDBLogType::DatabaseCreate:  // not handled here
      case RocksDBLogType::DatabaseDrop: {
        resetTransientState();  // finish ongoing trx
        break;
      }
      case RocksDBLogType::CollectionCreate:
        resetTransientState();  // finish ongoing trx
        if (shouldHandleCollection(RocksDBLogValue::databaseId(blob),
                                   RocksDBLogValue::collectionId(blob))) {
          _state = COLLECTION_CREATE;
        }
        break;
      case RocksDBLogType::CollectionRename:
        resetTransientState();  // finish ongoing trx
        if (shouldHandleCollection(RocksDBLogValue::databaseId(blob),
                                   RocksDBLogValue::collectionId(blob))) {
          _state = COLLECTION_RENAME;
          _oldCollectionName = RocksDBLogValue::oldCollectionName(blob).toString();
        }
        break;
      case RocksDBLogType::CollectionChange:
        resetTransientState();  // finish ongoing trx
        if (shouldHandleCollection(RocksDBLogValue::databaseId(blob),
                                   RocksDBLogValue::collectionId(blob))) {
          _state = COLLECTION_CHANGE;
        }
        break;
      case RocksDBLogType::CollectionDrop: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        // always print drop collection marker, shouldHandleCollection will
        // always return false for dropped collections
        if (shouldHandleDB(dbid)) {
          {  // tick number
            StringRef uuid = RocksDBLogValue::collectionUUID(blob);
            TRI_ASSERT(!uuid.empty());
            uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(tick)));
            marker->add("type", VPackValue(REPLICATION_COLLECTION_DROP));
            marker->add("database", VPackValue(std::to_string(dbid)));
            if (!uuid.empty()) {
              marker->add("cuid", VPackValuePair(uuid.data(), uuid.size(),
                                                 VPackValueType::String));
            }
            marker->add("cid", VPackValue(std::to_string(cid)));
            VPackObjectBuilder data(&_builder, "data", true);
            data->add("id", VPackValue(std::to_string(cid)));
            data->add("name", VPackValue(""));  // not used at all
          }
          updateLastEmittedTick(_currentSequence);
        }
        break;
      }
      case RocksDBLogType::CollectionTruncate: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          TRI_ASSERT(_vocbase->id() == dbid);
          LogicalCollection* coll = loadCollection(cid);
          TRI_ASSERT(coll != nullptr);
          {
            uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(tick)));
            marker->add("type", VPackValue(REPLICATION_COLLECTION_TRUNCATE));
            marker->add("database", VPackValue(std::to_string(dbid)));
            marker->add("cuid", VPackValue(coll->guid()));
            marker->add("cid", VPackValue(std::to_string(cid)));
          }
          updateLastEmittedTick(_currentSequence);
        }
        break;
      }
      case RocksDBLogType::IndexCreate: {
        resetTransientState();  // finish ongoing trx

        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);

        if (shouldHandleCollection(dbid, cid)) {
          TRI_ASSERT(_vocbase->id() == dbid);
          LogicalCollection* coll = loadCollection(cid);
          TRI_ASSERT(coll != nullptr);
          VPackSlice indexDef = RocksDBLogValue::indexSlice(blob);
          auto stripped = rocksutils::stripObjectIds(indexDef);
          uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);

          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(tick)));
          _builder.add("type", VPackValue(REPLICATION_INDEX_CREATE));
          _builder.add("database", VPackValue(std::to_string(dbid)));
          _builder.add("cid", VPackValue(std::to_string(cid)));
          _builder.add("cuid", VPackValue(coll->guid()));
          _builder.add("cname", VPackValue(coll->name()));
          _builder.add("data", stripped.first);
          _builder.close();
          updateLastEmittedTick(tick);
        }

        break;
      }
      case RocksDBLogType::IndexDrop: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
        // only print markers from this collection if it is set
        if (shouldHandleCollection(dbid, cid)) {
          TRI_ASSERT(_vocbase->id() == dbid);
          LogicalCollection* coll = loadCollection(cid);
          TRI_ASSERT(coll != nullptr);
          uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(tick)));
          _builder.add("type", VPackValue(REPLICATION_INDEX_DROP));
          _builder.add("database", VPackValue(std::to_string(dbid)));
          _builder.add("cid", VPackValue(std::to_string(cid)));
          _builder.add("cname", VPackValue(coll->name()));
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add("id", VPackValue(std::to_string(iid)));
          _builder.close();
          _builder.close();
          updateLastEmittedTick(tick);
        }
        break;
      }
      case RocksDBLogType::ViewCreate:
      case RocksDBLogType::ViewDrop:
      case RocksDBLogType::ViewChange: {
        resetTransientState();  // finish ongoing trx
        break;
      }
      case RocksDBLogType::BeginTransaction: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_tid_t tid = RocksDBLogValue::transactionId(blob);
        if (shouldHandleDB(dbid)) {
          _state = TRANSACTION;
          _currentTrxId = tid;
          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
          _builder.add("type", VPackValue(convertLogType(type)));
          _builder.add("database", VPackValue(std::to_string(dbid)));
          _builder.add("tid", VPackValue(std::to_string(tid)));
          _builder.close();
          updateLastEmittedTick(_currentSequence);
        }
        break;
      }
      case RocksDBLogType::CommitTransaction: {  // ideally optional
        if (_state == TRANSACTION) {
          TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
          TRI_voc_tid_t tid = RocksDBLogValue::transactionId(blob);
          TRI_ASSERT(_currentTrxId == tid && _vocbase->id() == dbid);
          if (shouldHandleDB(dbid) && _currentTrxId == tid) {
            writeCommitMarker();
          }
        }
        resetTransientState();
        break;
      }
      case RocksDBLogType::SinglePut: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          _state = SINGLE_PUT;
        }
        break;
      }
      case RocksDBLogType::SingleRemove: {  // deprecated
        resetTransientState();              // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          _state = SINGLE_REMOVE;  // revisionId is unknown
        }
        break;
      }
      case RocksDBLogType::DocumentRemoveV2: {  // remove within a trx
        if (_state == TRANSACTION) {
          TRI_ASSERT(_removedDocRid == 0);
          _removedDocRid = RocksDBLogValue::revisionId(blob);
        } else {
          resetTransientState();
        }
        break;
      }
      case RocksDBLogType::SingleRemoveV2: {
        resetTransientState();  // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          _state = SINGLE_REMOVE;
          _removedDocRid = RocksDBLogValue::revisionId(blob);
        }
        break;
      }

      case RocksDBLogType::DocumentOperationsPrologue:
      case RocksDBLogType::DocumentRemove:
      case RocksDBLogType::DocumentRemoveAsPartOfUpdate:
        break;  // ignore deprecated && unused markers

      default:
        LOG_TOPIC(WARN, Logger::REPLICATION)
            << "Unhandled wal log entry " << rocksDBLogTypeName(type);
        break;
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    tick();
    LOG_TOPIC(_LOG, Logger::REPLICATION)
        << "PUT: key:" << key.ToString() << "  value: " << value.ToString();

    if (column_family_id == _definitionsCF) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        TRI_voc_tick_t dbid = RocksDBKey::databaseId(key);
        TRI_voc_cid_t cid = RocksDBKey::collectionId(key);
        if (shouldHandleCollection(dbid, cid) &&
            (_state == COLLECTION_CREATE || _state == COLLECTION_RENAME ||
             _state == COLLECTION_CHANGE)) {
          TRI_ASSERT(_vocbase->id() == dbid);
          LogicalCollection* coll = loadCollection(cid);
          TRI_ASSERT(coll != nullptr);

          VPackSlice collectionDef = RocksDBValue::data(value);
          VPackObjectBuilder marker(&_builder, true);
          marker->add("tick", VPackValue(std::to_string(_currentSequence)));
          marker->add("database", VPackValue(std::to_string(dbid)));
          marker->add("cid", VPackValue(std::to_string(cid)));
          marker->add("cname", VPackValue(coll->name()));
          if (_state == COLLECTION_CREATE) {
            auto stripped = rocksutils::stripObjectIds(collectionDef);
            marker->add("type", VPackValue(REPLICATION_COLLECTION_CREATE));
            marker->add("data", stripped.first);
          } else if (_state == COLLECTION_RENAME) {
            marker->add("type", VPackValue(REPLICATION_COLLECTION_RENAME));
            VPackObjectBuilder data(&_builder, "data", true);
            data->add("name", VPackValue(coll->name()));
            data->add("oldName", VPackValue(_oldCollectionName));
            data->add("id", VPackValue(std::to_string(cid)));
          } else if (_state == COLLECTION_CHANGE) {
            auto stripped = rocksutils::stripObjectIds(collectionDef);
            marker->add("type", VPackValue(REPLICATION_COLLECTION_CHANGE));
            marker->add("data", stripped.first);
          }
          updateLastEmittedTick(_currentSequence);
        }
      }  // if (RocksDBKey::type(key) == RocksDBEntryType::Collection)

      // reset everything immediately after DDL operations
      resetTransientState();

    } else if (column_family_id == _documentsCF) {
      if (_state != TRANSACTION && _state != SINGLE_PUT) {
        resetTransientState();
        return rocksdb::Status();
      }
      TRI_ASSERT(_state != SINGLE_PUT || _currentTrxId == 0);
      TRI_ASSERT(_removedDocRid == 0);
      _removedDocRid = 0;

      uint64_t objectId = RocksDBKey::objectId(key);
      auto dbCollPair = rocksutils::mapObjectToCollection(objectId);
      TRI_voc_tick_t const dbid = dbCollPair.first;
      TRI_voc_cid_t const cid = dbCollPair.second;
      if (!shouldHandleCollection(dbid, cid)) {
        return rocksdb::Status();  // no reset here
      }
      TRI_ASSERT(_vocbase->id() == dbid);

      LogicalCollection* col = loadCollection(cid);
      TRI_ASSERT(col != nullptr);
      {
        VPackObjectBuilder marker(&_builder, true);
        marker->add("tick", VPackValue(std::to_string(_currentSequence)));
        marker->add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
        marker->add("database", VPackValue(std::to_string(dbid)));
        marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
        marker->add("cid", VPackValue(std::to_string(cid)));
        marker->add("cname", VPackValue(col->name()));
        marker->add("data", RocksDBValue::data(value));
      }
      updateLastEmittedTick(_currentSequence);

      if (_state == SINGLE_PUT) {
        resetTransientState();  // always reset after single op
      }
    }

    return rocksdb::Status();
  }

  // for Delete / SingleDelete
  void handleDeleteCF(uint32_t cfId, rocksdb::Slice const& key) {
    tick();

    if (cfId != _primaryCF) {
      return;  // ignore all document operations
    } else if (_state != TRANSACTION && _state != SINGLE_REMOVE) {
      resetTransientState();
      return;
    }
    TRI_ASSERT(_state != SINGLE_REMOVE || _currentTrxId == 0);

    uint64_t objectId = RocksDBKey::objectId(key);
    auto triple = rocksutils::mapObjectToIndex(objectId);
    TRI_voc_tick_t const dbid = std::get<0>(triple);
    TRI_voc_cid_t const cid = std::get<1>(triple);
    if (!shouldHandleCollection(dbid, cid)) {
      _removedDocRid = 0;  // ignore rid too
      return;              // no reset here
    }
    TRI_ASSERT(_vocbase->id() == dbid);

    StringRef docKey = RocksDBKey::primaryKey(key);
    LogicalCollection* coll = loadCollection(cid);
    TRI_ASSERT(coll != nullptr);
    {
      VPackObjectBuilder marker(&_builder, true);
      marker->add("tick", VPackValue(std::to_string(_currentSequence)));
      marker->add("type", VPackValue(REPLICATION_MARKER_REMOVE));
      marker->add("database", VPackValue(std::to_string(dbid)));
      marker->add("cid", VPackValue(std::to_string(cid)));
      marker->add("cname", VPackValue(coll->name()));
      marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
      VPackObjectBuilder data(&_builder, "data", true);
      data->add(StaticStrings::KeyString,
                VPackValuePair(docKey.data(), docKey.size(), VPackValueType::String));
      data->add(StaticStrings::RevString, VPackValue(TRI_RidToString(_removedDocRid)));
    }
    updateLastEmittedTick(_currentSequence);
    _removedDocRid = 0;  // always reset
    if (_state == SINGLE_REMOVE) {
      resetTransientState();
    }
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id, rocksdb::Slice const& key) override {
    handleDeleteCF(column_family_id, key);
    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id, rocksdb::Slice const& key) override {
    handleDeleteCF(column_family_id, key);
    return rocksdb::Status();
  }

  rocksdb::Status DeleteRangeCF(uint32_t /*column_family_id*/,
                                const rocksdb::Slice& /*begin_key*/,
                                const rocksdb::Slice& /*end_key*/) override {
    // nothing special to-do here. collection dropping and
    // truncation is already handled elsewhere
    return rocksdb::Status();
  }

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
    // reset all states
    _state = INVALID;
    _currentTrxId = 0;
    _removedDocRid = 0;
    _oldCollectionName.clear();
  }

  void writeCommitMarker() {
    TRI_ASSERT(_state == TRANSACTION);
    LOG_TOPIC(_LOG, Logger::REPLICATION) << "tick: " << _currentSequence << " commit transaction";

    _builder.openObject();
    _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
    _builder.add("type", VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_COMMIT)));
    _builder.add("database", VPackValue(std::to_string(_vocbase->id())));
    _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
    _builder.close();
    updateLastEmittedTick(_currentSequence);
    _state = INVALID;  // for safety
  }

  // should reset state flags which are only valid between
  // observing a specific log entry and a sequence of immediately
  // following PUT / DELETE / Log entries
  void resetTransientState() {
    // reset all states
    _state = INVALID;
    _currentTrxId = 0;
    _removedDocRid = 0;
    _oldCollectionName.clear();
  }

  uint64_t endBatch() {
    TRI_ASSERT(_removedDocRid == 0);
    TRI_ASSERT(_oldCollectionName.empty());
    resetTransientState();
    return _currentSequence;
  }

  rocksdb::SequenceNumber lastEmittedTick() const { return _lastEmittedTick; }

 private:
  void updateLastEmittedTick(rocksdb::SequenceNumber value) {
    // the tick values emitted should be always increasing
    // in the case of transaction we may see the same tick value as before, but
    // tick values must never decrease
    TRI_ASSERT(value >= _lastEmittedTick);
    _lastEmittedTick = value;
  }

  // tick function that is called before each new WAL entry
  void tick() {
    if (_startOfBatch) {
      // we are at the start of a batch. do NOT increase sequence number
      _startOfBatch = false;
    } else {
      // we are inside a batch already. now increase sequence number
      ++_currentSequence;
    }
  }

  bool shouldHandleDB(TRI_voc_tick_t dbid) const {
    return _vocbase->id() == dbid;
  }

  /// @brief Check if collection is in filter, will load collection
  bool shouldHandleCollection(TRI_voc_tick_t dbid, TRI_voc_cid_t cid) {
    if (dbid == 0 || cid == 0 || !shouldHandleDB(dbid)) {
      return false;
    }
    if (_onlyCollectionId == 0 || _onlyCollectionId == cid) {
      LogicalCollection* collection = loadCollection(cid);
      if (collection == nullptr) {
        return false;
      }
      return !TRI_ExcludeCollectionReplication(collection->name(), _includeSystem);
    }
    return false;
  }

  LogicalCollection* loadCollection(TRI_voc_cid_t cid) {
    TRI_ASSERT(cid != 0);
    if (_vocbase != nullptr) {
      auto const& it = _collectionCache.find(cid);
      if (it != _collectionCache.end()) {
        return it->second.collection();
      }

      auto collection = _vocbase->lookupCollection(cid);
      if (collection) {
        _collectionCache.emplace(cid, CollectionGuard(_vocbase, collection));
        return collection.get();
      }
    }

    return nullptr;
  }

 private:
  uint32_t const _definitionsCF;
  uint32_t const _documentsCF;
  uint32_t const _primaryCF;

  // these parameters are relevant to determine if we can print
  // a specific marker from the WAL
  TRI_vocbase_t* const _vocbase;
  // @brief collection replication UUID cache
  std::map<TRI_voc_cid_t, CollectionGuard> _collectionCache;
  bool const _includeSystem;
  TRI_voc_cid_t const _onlyCollectionId;

  /// result builder
  VPackBuilder& _builder;

  // Various state machine flags
  rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;
  rocksdb::SequenceNumber _lastEmittedTick;  // just used for validation
  bool _startOfBatch = false;

  // Various state machine flags
  State _state = INVALID;
  TRI_voc_tick_t _currentTrxId = 0;
  TRI_voc_rid_t _removedDocRid = 0;
  std::string _oldCollectionName;
};

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database
RocksDBReplicationResult rocksutils::tailWal(TRI_vocbase_t* vocbase, uint64_t tickStart,
                                             uint64_t tickEnd, size_t chunkSize,
                                             bool includeSystem, TRI_voc_cid_t collectionId,
                                             VPackBuilder& builder) {
  TRI_ASSERT(tickStart <= tickEnd);
  uint64_t lastTick = tickStart;         // generally contains begin of last wb
  uint64_t lastWrittenTick = tickStart;  // contains end tick of last wb
  uint64_t lastScannedTick = tickStart;

  // LOG_TOPIC(WARN, Logger::FIXME) << "1. Starting tailing: tickStart " <<
  // tickStart << " tickEnd " << tickEnd << " chunkSize " << chunkSize;//*/

  std::unique_ptr<WALParser> handler(
      new WALParser(vocbase, includeSystem, collectionId, builder));
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;

  rocksdb::Status s;
  // no need verifying the WAL contents
  rocksdb::TransactionLogIterator::ReadOptions ro(false);
  uint64_t since = 0;
  if (tickStart > 0) {
    since = tickStart - 1;
  }
  s = rocksutils::globalRocksDB()->GetUpdatesSince(since, &iterator, ro);

  if (!s.ok()) {
    auto converted = convertStatus(s, rocksutils::StatusHint::wal);

    TRI_ASSERT(converted.fail());
    TRI_ASSERT(converted.errorNumber() != TRI_ERROR_NO_ERROR);
    return {converted.errorNumber(), lastTick};
  }

  bool minTickIncluded = false;
  // we need to check if the builder is bigger than the chunksize,
  // only after we printed a full WriteBatch. Otherwise a client might
  // never read the full writebatch
  while (iterator->Valid() && lastTick <= tickEnd && builder.buffer()->size() < chunkSize) {
    s = iterator->status();
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "error during WAL scan: " << s.ToString();
      break;  // s is considered in the end
    }

    rocksdb::BatchResult batch = iterator->GetBatch();
    TRI_ASSERT(lastTick == tickStart || batch.sequence >= lastTick);

    if (batch.sequence <= tickEnd) {
      lastScannedTick = batch.sequence;
    }

    if (!minTickIncluded && batch.sequence <= tickStart && batch.sequence <= tickEnd) {
      minTickIncluded = true;
    }
    if (batch.sequence <= tickStart) {
      iterator->Next();  // skip
      continue;
    } else if (batch.sequence > tickEnd) {
      break;  // cancel out
    }

    lastTick = batch.sequence;
    LOG_TOPIC(_LOG, Logger::REPLICATION) << "Start WriteBatch tick: " << lastTick;
    handler->startNewBatch(batch.sequence);
    s = batch.writeBatchPtr->Iterate(handler.get());
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::REPLICATION) << "error during WAL scan: " << s.ToString();
      break;  // s is considered in the end
    }

    lastWrittenTick = handler->endBatch();
    LOG_TOPIC(_LOG, Logger::REPLICATION) << "End WriteBatch written-tick: " << lastWrittenTick;
    TRI_ASSERT(lastTick <= lastWrittenTick);
    if (!minTickIncluded && lastWrittenTick <= tickStart && lastWrittenTick <= tickEnd) {
      minTickIncluded = true;
    }
    iterator->Next();
  }

  RocksDBReplicationResult result(TRI_ERROR_NO_ERROR, lastWrittenTick);
  result.lastScannedTick(lastScannedTick);
  if (!s.ok()) {  // TODO do something?
    result.reset(convertStatus(s, rocksutils::StatusHint::wal));
  }
  if (minTickIncluded) {
    result.includeMinTick();
  }

  TRI_ASSERT(!result.ok() || (result.maxTick() >= handler->lastEmittedTick()));
  // LOG_TOPIC(WARN, Logger::FIXME) << "2.  lastWrittenTick: " <<
  // lastWrittenTick;
  return result;
}
