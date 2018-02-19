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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBWalAccess.h"
#include "Basics/StaticStrings.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"

#include "Logger/Logger.h"

#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Builder.h>

using namespace arangodb;

/// {"tickMin":"123", "tickMax":"456", "version":"3.2", "serverId":"abc"}
Result RocksDBWalAccess::tickRange(
    std::pair<TRI_voc_tick_t, TRI_voc_tick_t>& minMax) const {
  rocksdb::TransactionDB* tdb = rocksutils::globalRocksDB();
  rocksdb::VectorLogPtr walFiles;
  rocksdb::Status s = tdb->GetSortedWalFiles(walFiles);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  if (walFiles.size() > 0) {
    minMax.first = walFiles.front()->StartSequence();
  }
  minMax.second = tdb->GetLatestSequenceNumber();
  return TRI_ERROR_NO_ERROR;
}

/// {"lastTick":"123",
///  "version":"3.2",
///  "serverId":"abc",
///  "clients": {
///    "serverId": "ass", "lastTick":"123", ...
///  }}
///
TRI_voc_tick_t RocksDBWalAccess::lastTick() const {
  rocksutils::globalRocksEngine()->flushWal(false, false, false);
  return rocksutils::globalRocksDB()->GetLatestSequenceNumber();
}

/// should return the list of transactions started, but not committed in that
/// range (range can be adjusted)
WalAccessResult RocksDBWalAccess::openTransactions(
    uint64_t tickStart, uint64_t tickEnd, WalAccess::Filter const& filter,
    TransactionCallback const&) const {
  return WalAccessResult(TRI_ERROR_NO_ERROR, true, 0, 0, 0);
}

/// WAL parser. Premise of this code is that transactions
/// can potentially be batched into the same rocksdb write batch
/// but transactions can never be interleaved with operations
/// outside of the transaction
class MyWALParser : public rocksdb::WriteBatch::Handler, public WalAccessContext {
  
 public:
  MyWALParser(WalAccess::Filter const& filter,
              WalAccess::MarkerCallback const& f)
      : WalAccessContext(filter, f),
        _definitionsCF(RocksDBColumnFamily::definitions()->GetID()),
        _documentsCF(RocksDBColumnFamily::documents()->GetID()),
        _primaryCF(RocksDBColumnFamily::primary()->GetID()),
        _startSequence(0),
        _currentSequence(0) {}

  void LogData(rocksdb::Slice const& blob) override {
    // rocksdb does not count LogData towards sequence-number
    RocksDBLogType type = RocksDBLogValue::type(blob);
    
    //LOG_TOPIC(ERR, Logger::FIXME) << "[LOG] " << _currentSequence
    //  << " " << rocksDBLogTypeName(type);
    switch (type) {
      case RocksDBLogType::DatabaseCreate:
        resetTransientState(); // finish ongoing trx
        if (shouldHandleDB(RocksDBLogValue::databaseId(blob))) {
          _state = DB_CREATE;
        }
        // wait for marker data in Put entry
        break;
      case RocksDBLogType::DatabaseDrop:
        resetTransientState(); // finish ongoing trx
        if (shouldHandleDB(RocksDBLogValue::databaseId(blob))) {
          _state = DB_DROP;
        }
        // wait for marker data in Put entry
        break;
      case RocksDBLogType::CollectionCreate:
      case RocksDBLogType::CollectionRename:
      case RocksDBLogType::CollectionChange:
        resetTransientState(); // finish ongoing trx
        if (shouldHandleCollection(RocksDBLogValue::databaseId(blob),
                                   RocksDBLogValue::collectionId(blob))) {
          _state = (State)type;
          TRI_ASSERT(_state == COLLECTION_CREATE ||
                     _state == COLLECTION_RENAME ||
                     _state == COLLECTION_CHANGE);
        }
        break;
      case RocksDBLogType::CollectionDrop: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        // always print drop collection marker, shouldHandleCollection will
        // always return false for dropped collections
        if (shouldHandleDB(dbid)) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          if (vocbase != nullptr) {
            { // tick number
              StringRef uuid = RocksDBLogValue::collectionUUID(blob);
              TRI_ASSERT(!uuid.empty());
              uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
              VPackObjectBuilder marker(&_builder, true);
              marker->add("tick", VPackValue(std::to_string(tick)));
              marker->add("type", VPackValue(rocksutils::convertLogType(type)));
              marker->add("db", VPackValue(vocbase->name()));
              marker->add("cuid", VPackValuePair(uuid.data(), uuid.size(),
                                                 VPackValueType::String));
            }
            _callback(vocbase, _builder.slice());
            _responseSize += _builder.size();
            _builder.clear();
          }
        }
        break;
      }
      case RocksDBLogType::IndexCreate: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        // only print markers from this collection if it is set
        if (shouldHandleCollection(dbid, cid)) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          LogicalCollection* col = loadCollection(dbid, cid);
          TRI_ASSERT(vocbase != nullptr && col != nullptr);
            {
              uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
              VPackObjectBuilder marker(&_builder, true);
              marker->add("tick", VPackValue(std::to_string(tick)));
              marker->add("type", VPackValue(rocksutils::convertLogType(type)));
              marker->add("db", VPackValue(vocbase->name()));
              marker->add("cuid", VPackValue(col->globallyUniqueId()));
              marker->add("data", RocksDBLogValue::indexSlice(blob));
            }
            _callback(vocbase, _builder.slice());
            _responseSize += _builder.size();
            _builder.clear();
        }
        break;
      }
      case RocksDBLogType::IndexDrop: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
        // only print markers from this collection if it is set
        if (shouldHandleCollection(dbid, cid)) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          LogicalCollection* col = loadCollection(dbid, cid);
          TRI_ASSERT(vocbase != nullptr && col != nullptr);
          {
            uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(tick)));
            marker->add("type", VPackValue(rocksutils::convertLogType(type)));
            marker->add("db", VPackValue(vocbase->name()));
            marker->add("cuid", VPackValue(col->globallyUniqueId()));
            VPackObjectBuilder data(&_builder, "data", true);
            data->add("id", VPackValue(std::to_string(iid)));
          }
          _callback(vocbase, _builder.slice());
          _responseSize += _builder.size();
          _builder.clear();
        }
        break;
      }
      case RocksDBLogType::ViewCreate:
      case RocksDBLogType::ViewDrop:
      case RocksDBLogType::ViewChange:
      case RocksDBLogType::ViewRename: {
        resetTransientState(); // finish ongoing trx
        // TODO
        break;
      }
      case RocksDBLogType::BeginTransaction: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tid_t tid = RocksDBLogValue::transactionId(blob);
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        if (shouldHandleDB(dbid)) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          if (vocbase != nullptr) {
            _state = TRANSACTION;
            _currentTrxId = tid;
            _trxDbId = dbid;
            {
              VPackObjectBuilder marker(&_builder, true);
              marker->add("tick", VPackValue(std::to_string(_currentSequence)));
              marker->add("type", VPackValue(rocksutils::convertLogType(type)));
              marker->add("db", VPackValue(vocbase->name()));
              marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
            }
            _callback(vocbase, _builder.slice());
            _responseSize += _builder.size();
            _builder.clear();
          }
        }
        
        break;
      }
      
      /*case RocksDBLogType::DocumentOperationsPrologue: {
        // part of an ongoing transaction
        if (_currentDbId != 0 && _currentTrxId != 0) {
          // database (and therefore transaction) may be ignored
          TRI_ASSERT(_seenBeginTransaction && !_singleOp);
          // document ops can ignore this collection later
          _currentCid = RocksDBLogValue::collectionId(blob);
        }
        break;
      }
      case RocksDBLogType::DocumentRemove: 
      case RocksDBLogType::DocumentRemoveAsPartOfUpdate: {
        // part of an ongoing transaction
        if (_currentDbId != 0 && _currentTrxId != 0) {
          // collection may be ignored
          TRI_ASSERT(_seenBeginTransaction && !_singleOp);
          if (shouldHandleCollection(_currentDbId, _currentCid)) {
            _removeDocumentKey = RocksDBLogValue::documentKey(blob).toString();
          }
        }
        break;
      }*/
        
      case RocksDBLogType::SinglePut: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          _state = SINGLE_PUT;
        }
        break;
      }
      case RocksDBLogType::SingleRemove: {
        resetTransientState(); // finish ongoing trx
        TRI_voc_tick_t dbid = RocksDBLogValue::databaseId(blob);
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        if (shouldHandleCollection(dbid, cid)) {
          _state = SINGLE_REMOVE;
        }
        break;
      }
      case RocksDBLogType::CommitTransaction:
        resetTransientState(); // finish ongoing trx
        break;

      default:
        LOG_TOPIC(WARN, Logger::REPLICATION) << "Unhandled wal log entry "
                                             << rocksDBLogTypeName(type);
        break;
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    tick();
    //LOG_TOPIC(ERR, Logger::ROCKSDB) << "[PUT] cf: " << column_family_id
    // << ", key:" << key.ToString() << "  value: " << value.ToString();

    if (column_family_id == _definitionsCF) {
      
      // LogData should have triggered a commit on ongoing transactions
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        // database slice should contain "id" and "name"
        VPackSlice const data = RocksDBValue::data(value);
        VPackSlice const name = data.get("name");
        TRI_ASSERT(name.isString() && name.getStringLength() > 0);

        TRI_voc_tick_t dbid = RocksDBKey::databaseId(key);
        if (_state == DB_CREATE) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          if (vocbase != nullptr) { // db is already deleted
            {
              VPackObjectBuilder marker(&_builder, true);
              marker->add("tick", VPackValue(std::to_string(_currentSequence)));
              marker->add("type", VPackValue(REPLICATION_DATABASE_CREATE));
              marker->add("db", name);
              marker->add("data", data);
            }
            _callback(vocbase, _builder.slice());
            _responseSize += _builder.size();
            _builder.clear();
          }
        } else if (_state == DB_DROP) {
          // prepareDropDatabase should always write entry
          VPackSlice const del = data.get("deleted");
          TRI_ASSERT(del.isBool() && del.getBool());
          {
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(_currentSequence)));
            marker->add("type", VPackValue(REPLICATION_DATABASE_DROP));
            marker->add("db", name);
          }
          _callback(loadVocbase(dbid), _builder.slice());
          _responseSize += _builder.size();
          _builder.clear();
        } // ignore Put in any other case
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        
        TRI_voc_tick_t dbid = RocksDBKey::databaseId(key);
        TRI_voc_cid_t cid = RocksDBKey::collectionId(key);
        if (shouldHandleCollection(dbid, cid) && (_state == COLLECTION_CREATE ||
                                                  _state == COLLECTION_RENAME ||
                                                  _state == COLLECTION_CHANGE)) {
          TRI_vocbase_t* vocbase = loadVocbase(dbid);
          LogicalCollection* col = loadCollection(dbid, cid);
          TRI_ASSERT(vocbase != nullptr && col != nullptr);
          {
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(_currentSequence)));
            marker->add("db", VPackValue(vocbase->name()));
            marker->add("cuid", VPackValue(col->globallyUniqueId()));
            if (_state == COLLECTION_CREATE) {
              marker->add("type", VPackValue(REPLICATION_COLLECTION_CREATE));
              marker->add("data", RocksDBValue::data(value));
            } else if (_state == COLLECTION_RENAME) {
              marker->add("type", VPackValue(REPLICATION_COLLECTION_RENAME));
              VPackObjectBuilder data(&_builder, "data", true);
              data->add("name", VPackValue(col->name()));
            } else if (_state == COLLECTION_CHANGE) {
              marker->add("type", VPackValue(REPLICATION_COLLECTION_CHANGE));
              marker->add("data", RocksDBValue::data(value));
            }
          }
          _callback(vocbase, _builder.slice());
          _responseSize += _builder.size();
          _builder.clear();
        }
      } // if (RocksDBKey::type(key) == RocksDBEntryType::Collection)
      
      // reset everything immediately after DDL operation
      resetTransientState();

    } else if (column_family_id == _documentsCF) {
      
      if (_state != TRANSACTION && _state != SINGLE_PUT) {
        resetTransientState();
        return rocksdb::Status();
      }
      
      uint64_t objectId = RocksDBKey::objectId(key);
      auto dbCollPair = rocksutils::mapObjectToCollection(objectId);
      TRI_voc_tick_t const dbid = dbCollPair.first;
      TRI_voc_cid_t const cid = dbCollPair.second;
      if (!shouldHandleCollection(dbid, cid)) {
        return rocksdb::Status(); // no reset here
      }

      TRI_vocbase_t* vocbase = loadVocbase(dbid);
      LogicalCollection* col = loadCollection(dbid, cid);
      TRI_ASSERT(vocbase != nullptr && col != nullptr);
      {
        VPackObjectBuilder marker(&_builder, true);
        marker->add("tick", VPackValue(std::to_string(_currentSequence)));
        marker->add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
        marker->add("db", VPackValue(vocbase->name()));
        marker->add("cuid", VPackValue(col->globallyUniqueId()));
        marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
        marker->add("data", RocksDBValue::data(value));
      }
      _callback(vocbase, _builder.slice());
      _responseSize += _builder.size();
      _builder.clear();

      if (_state == SINGLE_PUT) {
        resetTransientState(); // always reset after single op
      }
    }
    return rocksdb::Status();
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id,
                           rocksdb::Slice const& key) override {
    return handleDeletion(column_family_id, key);
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id,
                                 rocksdb::Slice const& key) override {
    return handleDeletion(column_family_id, key);
  }

  rocksdb::Status handleDeletion(uint32_t column_family_id,
                                 rocksdb::Slice const& key) {
    tick();
/*#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (column_family_id == _definitionsCF &&
        shouldHandleMarker(column_family_id, false, key)) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        // databases are deleted when the last reference to it disappears
        // the definitions entry deleted later in a cleanup thread
        TRI_ASSERT(false);
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionDrop);
        TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
        // we already printed this marker upon reading the log value
      }
    }
#endif
    
    if (column_family_id != _documentsCF ||
        !shouldHandleMarker(column_family_id, false, key)) {
      if (column_family_id == _documentsCF) {
        if (_lastLogType == RocksDBLogType::SingleRemove) {
          TRI_ASSERT(!_seenBeginTransaction);
          resetTransientState(); // ignoring the entire op
        } else {
          TRI_ASSERT(!_singleOp);
          _removeDocumentKey.clear(); // just ignoring this key
        }
      }
      
      return rocksdb::Status();
    }
    
    if (_lastLogType == RocksDBLogType::DocumentRemoveAsPartOfUpdate) {
      _removeDocumentKey.clear();
      return rocksdb::Status();
    }
    
    //LOG_TOPIC(ERR, Logger::ROCKSDB) << "[Delete] cf: " << column_family_id
    //<< " key:" << key.ToString();
    
    // document removes, because of a db / collection drop is not transactional
    //  and should not appear in the WAL.
    if (!(_seenBeginTransaction || _singleOp) ||
        (_lastLogType != RocksDBLogType::DocumentRemove &&
         _lastLogType != RocksDBLogType::SingleRemove)) {
      TRI_ASSERT(_removeDocumentKey.empty());
      // collection drops etc may be batched directly after a transaction
      // single operation updates in the WAL are wrongly ordered pre 3.3:
      // [..., LogType::SinglePut, DELETE old, PUT new, ...]
      if (_lastLogType != RocksDBLogType::SinglePut) {
        resetTransientState(); // finish ongoing trx
      }
      return rocksdb::Status();
    }
    
    TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
    TRI_ASSERT(!_singleOp || _currentTrxId == 0);
    TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
    TRI_ASSERT(!_removeDocumentKey.empty());*/
    
    if (column_family_id != _primaryCF) {
      return rocksdb::Status();
    }
    
    uint64_t objectId = RocksDBKey::objectId(key);
    auto dbCollPair = rocksutils::mapObjectToCollection(objectId);
    TRI_voc_tick_t const dbid = dbCollPair.first;
    TRI_voc_cid_t const cid = dbCollPair.second;
    if (!shouldHandleCollection(dbid, cid)) {
      return rocksdb::Status(); // no reset here
    }
    
    StringRef docKey = RocksDBKey::primaryKey(key);
    
    TRI_vocbase_t* vocbase = loadVocbase(objectId);
    LogicalCollection* col = loadCollection(objectId, cid);
    TRI_ASSERT(vocbase != nullptr && col != nullptr);
    {
      VPackObjectBuilder marker(&_builder, true);
      marker->add("tick", VPackValue(std::to_string(_currentSequence)));
      marker->add("type", VPackValue(REPLICATION_MARKER_REMOVE));
      marker->add("db", VPackValue(vocbase->name()));
      marker->add("cuid", VPackValue(col->globallyUniqueId()));
      marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
      VPackObjectBuilder data(&_builder, "data", true);
      data->add(StaticStrings::KeyString, VPackValuePair(docKey.data(), docKey.size(),
                                                         VPackValueType::String));
      if (_removedRid != 0) {
        data->add(StaticStrings::RevString, VPackValue(TRI_RidToString(_removedRid)));
      }
    }
    _callback(vocbase, _builder.slice());
    _responseSize += _builder.size();
    _builder.clear();
    _removedRid = 0;
    if (_state == SINGLE_REMOVE) {
      resetTransientState();
    }
    
    return rocksdb::Status();
  }

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
    _state = INVALID;
    _currentTrxId = 0;
    _trxDbId = 0;
    _removedRid = 0;
  }
                      
  void writeCommitMarker() {
    TRI_ASSERT(_state == TRANSACTION);
    TRI_vocbase_t* vocbase = loadVocbase(_trxDbId);
    if (vocbase != nullptr) { // we be in shutdown
      _builder.openObject(true);
      _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
      _builder.add("type", VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_COMMIT)));
      _builder.add("db", VPackValue(vocbase->name()));
      _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
      _builder.close();
      _callback(vocbase, _builder.slice());
      _responseSize += _builder.size();
      _builder.clear();
    }
    _state = INVALID;
  }

  // should reset state flags which are only valid between
  // observing a specific log entry and a sequence of immediately
  // following PUT / DELETE / Log entries
  void resetTransientState() {
    if (_state == TRANSACTION) {
      writeCommitMarker();
    }
    // reset all states
    _state = INVALID;
    _currentTrxId = 0;
    _trxDbId = 0;
    _removedRid = 0;
  }

  uint64_t endBatch() {
    TRI_ASSERT(_removedRid == 0);
    resetTransientState();
    return _currentSequence;
  }

  size_t responseSize() const { return _responseSize; }

 private:
  
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

  /*bool shouldHandleMarker(uint32_t column_family_id,
                          bool isPut, rocksdb::Slice const& key) {
    TRI_voc_tick_t dbId = 0;
    TRI_voc_cid_t cid = 0;
    if (column_family_id == _definitionsCF) {
      // only a PUT should be handled here anyway
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        return isPut && shouldHandleDB(RocksDBKey::databaseId(key));
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        //  || RocksDBKey::type(key) == RocksDBEntryType::View
        dbId = RocksDBKey::databaseId(key);
        cid = RocksDBKey::collectionId(key);
        if (!isPut || dbId == 0 || cid == 0) {
          // FIXME: seems broken to get a key with zero entries here
          return false;
        }
      } else {
        return false;
      }
    } else if (column_family_id == _documentsCF) {
      dbId = _currentDbId;
      cid = _currentCid;
      // happens when dropping a collection or log markers
      // are ignored for dbs and collections
      if (!(_seenBeginTransaction || _singleOp)) {
        return false;
      }
    } else {
      return false;
    }
    if (!shouldHandleCollection(dbId, cid)) {
      return false;
    }

    if (_lastLogType != RocksDBLogType::CollectionDrop) {
      // no document removes of dropped collections / vocbases
      TRI_vocbase_t* vocbase = loadVocbase(dbId);
      if (vocbase == nullptr) {
        return false;
      }
      std::string const collectionName = vocbase->collectionName(cid);
      if (collectionName.empty()) {
        return false;
      }
      if (!_filter.includeSystem && collectionName[0] == '_') {
        return false;
      }
      if (TRI_ExcludeCollectionReplication(collectionName,
                                           _filter.includeSystem)) {
        return false;
      }
    }
    return true;
  }*/

enum State : char {
  INVALID = 0,
  DB_CREATE = '1',
  DB_DROP = '2',
  COLLECTION_CREATE = '3',
  COLLECTION_DROP = '4',
  COLLECTION_RENAME = '5',
  COLLECTION_CHANGE = '6',
  INDEX_CREATE = '7',
  INDEX_DROP = '8',
  VIEW_CREATE = '9',
  VIEW_DROP = ':',
  VIEW_CHANGE = ';',
  TRANSACTION = '<',
  SINGLE_PUT = '?',
  SINGLE_REMOVE = '@',
  VIEW_RENAME = 'B',
};

 private:
  uint32_t const _definitionsCF;
  uint32_t const _documentsCF;
  uint32_t const _primaryCF;

  rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;

  // Various state machine flags
  bool _startOfBatch = false;
  State _state = INVALID;
  TRI_voc_tick_t _currentTrxId = 0;
  TRI_voc_tick_t _trxDbId = 0;
  TRI_voc_rid_t _removedRid = 0;
};

// iterates over WAL starting at 'from' and returns up to 'chunkSize' documents
// from the corresponding database
WalAccessResult RocksDBWalAccess::tail(uint64_t tickStart, uint64_t tickEnd,
                                       size_t chunkSize,
                                       TRI_voc_tick_t barrierId,
                                       Filter const& filter,
                                       MarkerCallback const& func) const {
  TRI_ASSERT(filter.transactionIds.empty());  // not supported in any way
  /*LOG_TOPIC(WARN, Logger::FIXME) << "1. Starting tailing: tickStart " <<
  tickStart << " tickEnd " << tickEnd << " chunkSize " << chunkSize;//*/
  
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();
  uint64_t firstTick = UINT64_MAX;      // first tick actually read
  uint64_t lastTick = tickStart;        // lastTick at start of a write batch
  uint64_t lastScannedTick = tickStart; // last tick we looked at
  uint64_t lastWrittenTick = 0;         // lastTick at the end of a write batch
  uint64_t latestTick = db->GetLatestSequenceNumber();

  auto handler = std::make_unique<MyWALParser>(filter, func);
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();

  rocksdb::Status s;
  // no need verifying the WAL contents
  rocksdb::TransactionLogIterator::ReadOptions ro(false);
  s = db->GetUpdatesSince(tickStart, &iterator, ro);
  if (!s.ok()) {
    Result r = convertStatus(s, rocksutils::StatusHint::wal);
    return WalAccessResult(r.errorNumber(), tickStart == latestTick,
                           0, 0, latestTick);
  }

  if (chunkSize < 16384) {
    // we need to have some sensible minimum
    chunkSize = 16384;
  }

  // we need to check if the builder is bigger than the chunksize,
  // only after we printed a full WriteBatch. Otherwise a client might
  // never read the full writebatch
  LOG_TOPIC(DEBUG, Logger::ROCKSDB) << "WAL tailing call. tick start: " << tickStart << ", tick end: " << tickEnd << ", chunk size: " << chunkSize;
  while (iterator->Valid() && lastTick <= tickEnd &&
         handler->responseSize() < chunkSize) {
    s = iterator->status();
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ROCKSDB) << "error during WAL scan: "
                                      << s.ToString();
      break;  // s is considered in the end
    }
  
    rocksdb::BatchResult batch = iterator->GetBatch();
    // record the first tick we are actually considering
    if (firstTick == UINT64_MAX) {
      firstTick = batch.sequence;
    }
    if (batch.sequence <= tickEnd) {
      lastScannedTick = batch.sequence;
    }
    
    //LOG_TOPIC(INFO, Logger::FIXME) << "found batch-seq: " << batch.sequence;
    lastTick = batch.sequence;  // start of the batch
    if (batch.sequence <= tickStart) {
      iterator->Next();  // skip
      continue;
    } else if (batch.sequence > tickEnd) {
      break;  // cancel out
    }

    handler->startNewBatch(batch.sequence);
    s = batch.writeBatchPtr->Iterate(handler.get());

    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ROCKSDB) << "error during WAL scan: "
                                      << s.ToString();
      break;  // s is considered in the end
    }
    lastWrittenTick = handler->endBatch();  // end of the batch

    TRI_ASSERT(lastWrittenTick >= lastTick);
    iterator->Next();
  }

  WalAccessResult result(TRI_ERROR_NO_ERROR, firstTick <= tickStart,
                         lastWrittenTick, lastScannedTick, latestTick);
  if (!s.ok()) {
    result.Result::reset(convertStatus(s, rocksutils::StatusHint::wal));
  }
  //LOG_TOPIC(WARN, Logger::FIXME) << "2. firstTick: " << firstTick << " lastWrittenTick: " << lastWrittenTick
  //<< " latestTick: " << latestTick;
  return result;
}
