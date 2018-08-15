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
#include "RocksDBEngine/RocksDBColumnFamily.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

// define to INFO to see the WAL output
#define _LOG TRACE

static std::string const emptyString;
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

    default:
      TRI_ASSERT(false);
      return REPLICATION_INVALID;
  }
}

/// WAL parser
class WALParser : public rocksdb::WriteBatch::Handler {
 public:
  WALParser(TRI_vocbase_t* vocbase, bool includeSystem,
            TRI_voc_cid_t collectionId, VPackBuilder& builder)
      : _documentsCF(RocksDBColumnFamily::documents()->GetID()),
        _definitionsCF(RocksDBColumnFamily::definitions()->GetID()),
        _vocbase(vocbase),
        _includeSystem(includeSystem),
        _onlyCollectionId(collectionId),
        _builder(builder),
        _startSequence(0),
        _currentSequence(0) {}

  void LogData(rocksdb::Slice const& blob) override {
    RocksDBLogType type = RocksDBLogValue::type(blob);
    TRI_DEFER(_lastLogType = type);

    // skip ignored databases and collections
    if (RocksDBLogValue::containsDatabaseId(type)) {
      TRI_voc_tick_t dbId = RocksDBLogValue::databaseId(blob);
      _currentDbId = dbId;
      if (!shouldHandleDB(dbId)) {
        resetTransientState();
        return;
      }
      if (RocksDBLogValue::containsCollectionId(type)) {
        TRI_voc_cid_t cid = RocksDBLogValue::collectionId(blob);
        _currentCid = cid;
        if (!shouldHandleCollection(dbId, cid)) {
          if (type == RocksDBLogType::SingleRemove || type == RocksDBLogType::SinglePut) {
            resetTransientState();
          } else {
            _currentCid = 0;
          }
          return;
        }
      }
    }
    
    LOG_TOPIC(_LOG, Logger::ROCKSDB) << "[LOG] " << rocksDBLogTypeName(type);
    switch (type) {
      case RocksDBLogType::DatabaseCreate:
      case RocksDBLogType::DatabaseDrop: {
        resetTransientState(); // finish ongoing trx
        _currentDbId = RocksDBLogValue::databaseId(blob);
        break;
      }
      case RocksDBLogType::CollectionRename:
      case RocksDBLogType::CollectionCreate:
      case RocksDBLogType::CollectionChange:
      case RocksDBLogType::CollectionDrop: {
        resetTransientState(); // finish ongoing trx
        if (_lastLogType == RocksDBLogType::IndexCreate) {
          TRI_ASSERT(_currentDbId == RocksDBLogValue::databaseId(blob));
          TRI_ASSERT(_currentCid == RocksDBLogValue::collectionId(blob));
        }
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        if (type == RocksDBLogType::CollectionRename) {
          _oldCollectionName = RocksDBLogValue::oldCollectionName(blob).toString();
        } else if (type == RocksDBLogType::CollectionDrop) {
          std::string UUID = RocksDBLogValue::collectionUUID(blob).toString();
          TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
          LOG_TOPIC(_LOG, Logger::ROCKSDB) << "CID: " << _currentCid;
          
          // reset name in collection name cache
          _collectionNames.erase(_currentCid);
          
          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
          _builder.add("type", VPackValue(REPLICATION_COLLECTION_DROP));
          _builder.add("database", VPackValue(std::to_string(_currentDbId)));
          if (!UUID.empty()) {
            _builder.add("cuid", VPackValue(UUID));
          }
          _builder.add("cid", VPackValue(std::to_string(_currentCid)));
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add("id", VPackValue(std::to_string(_currentCid)));
          _builder.add("name", VPackValue(""));  // not used at all
          _builder.close();
          _builder.close();
        }
        break;
      }
      case RocksDBLogType::IndexCreate: {
        resetTransientState(); // finish ongoing trx
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        // only print markers from this collection if it is set
        if (_onlyCollectionId == 0 || _currentCid == _onlyCollectionId) {
          uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
          VPackSlice indexSlice = RocksDBLogValue::indexSlice(blob);
          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(tick)));
          _builder.add("type", VPackValue(convertLogType(type)));
          _builder.add("database", VPackValue(std::to_string(_currentDbId)));
          _builder.add("cid", VPackValue(std::to_string(_currentCid)));
          std::string const& cname = nameFromCid(_currentCid);
          if (!cname.empty()) {
            _builder.add("cname", VPackValue(cname));
          }
          _builder.add("data", indexSlice);
          _builder.close();
        }
        break;
      }
      case RocksDBLogType::IndexDrop: {
        resetTransientState(); // finish ongoing trx
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
        // only print markers from this collection if it is set
        if (_onlyCollectionId == 0 || _currentCid == _onlyCollectionId) {
          uint64_t tick = _currentSequence + (_startOfBatch ? 0 : 1);
          _builder.openObject();
          _builder.add("tick", VPackValue(std::to_string(tick)));
          _builder.add("type", VPackValue(convertLogType(type)));
          _builder.add("database", VPackValue(std::to_string(_currentDbId)));
          _builder.add("cid", VPackValue(std::to_string(_currentCid)));
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add("id", VPackValue(std::to_string(iid)));
          _builder.close();
          _builder.close();
        }
        break;
      }
      case RocksDBLogType::ViewCreate:
      case RocksDBLogType::ViewDrop:
      case RocksDBLogType::ViewChange: {
        resetTransientState(); // finish ongoing trx
        // TODO
        break;
      }
      case RocksDBLogType::BeginTransaction: {
        TRI_ASSERT(!_singleOp);
        resetTransientState(); // finish ongoing trx
        _seenBeginTransaction = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentTrxId = RocksDBLogValue::transactionId(blob);
        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add("type", VPackValue(convertLogType(type)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
        _builder.close();
        break;
      }
      case RocksDBLogType::DocumentOperationsPrologue: {
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
        if (_currentDbId != 0 && _currentTrxId != 0 && _currentCid != 0) {
          // collection may be ignored
          TRI_ASSERT(_seenBeginTransaction && !_singleOp);
          if (shouldHandleCollection(_currentDbId, _currentCid)) {
            _removeDocumentKey = RocksDBLogValue::documentKey(blob).toString();
          }
        }
        break;
      }
      case RocksDBLogType::SingleRemove: {
        // we can only get here if we can handle this collection
        resetTransientState(); // finish ongoing trx
        _removeDocumentKey = RocksDBLogValue::documentKey(blob).toString();
        _singleOp = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        break;
      }
      case RocksDBLogType::SinglePut: {
        resetTransientState(); // finish ongoing trx
        _singleOp = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        break;
      }

      default:
        break;
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    tick();
    if (!shouldHandleKey(column_family_id, true, key)) {
      return rocksdb::Status();
    }
    LOG_TOPIC(_LOG, Logger::ROCKSDB) << "PUT: key:" << key.ToString()
                                     << "  value: " << value.ToString();
    
    if (column_family_id == _definitionsCF) {
      
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        TRI_ASSERT(_lastLogType == RocksDBLogType::DatabaseCreate ||
                   _lastLogType == RocksDBLogType::DatabaseDrop);
        // this version of the protocol will always ignore database markers
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        if (_lastLogType == RocksDBLogType::IndexCreate ||
            _lastLogType == RocksDBLogType::IndexDrop) {
          _lastLogType = RocksDBLogType::Invalid;
          return rocksdb::Status();
        }
        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionCreate ||
                   _lastLogType == RocksDBLogType::CollectionChange ||
                   _lastLogType == RocksDBLogType::CollectionRename);
        TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
        LOG_TOPIC(_LOG, Logger::ROCKSDB) << "CID: " << _currentCid;
        
        VPackSlice data = RocksDBValue::data(value);
        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add("type", VPackValue(convertLogType(_lastLogType)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCid)));
        
        VPackSlice cname = data.get("name");
        if (cname.isString()) {
          _builder.add("cname", cname);
          // set name in cache
          _collectionNames[_currentCid] = cname.copyString();
        }
        
        if (_lastLogType == RocksDBLogType::CollectionRename) {
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add("id", VPackValue(std::to_string(_currentCid)));
          _builder.add("oldName", VPackValue(_oldCollectionName));
          _builder.add("name", cname);
          _builder.close();
        } else {  // change and create need full data
          _builder.add("data", data);
        }
        _builder.close();
        // log type is only ever relevant, immediately after it appeared
        // we want double occurences create / drop / change collection to fail
        _lastLogType = RocksDBLogType::Invalid;
        _currentDbId = 0;
        _currentCid = 0;
      } // if (RocksDBKey::type(key) == RocksDBEntryType::Collection)
      
    } else if (column_family_id == _documentsCF) {
      TRI_ASSERT((_seenBeginTransaction && !_singleOp) ||
                 (!_seenBeginTransaction && _singleOp));
      // if real transaction, we need the trx id
      TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
      TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
      _builder.openObject();
      _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
      _builder.add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
      // auto containers = getContainerIds(key);
      _builder.add("database", VPackValue(std::to_string(_currentDbId)));
      _builder.add("cid", VPackValue(std::to_string(_currentCid)));
      // collection name
      std::string const& cname = nameFromCid(_currentCid);
      if (!cname.empty()) {
        _builder.add("cname", VPackValue(cname));
      }
      _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
      _builder.add("data", RocksDBValue::data(value));
      _builder.close();
      if (_singleOp) { // reset state immediately
        resetTransientState();
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
    if (!shouldHandleKey(column_family_id, false, key) ||
        column_family_id != _documentsCF) {
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
    
    // document removes, because of a collection drop is not transactional and
    // should not appear in the WAL.
    if (!(_seenBeginTransaction || _singleOp)) {
      return rocksdb::Status();
    } else if (_lastLogType != RocksDBLogType::DocumentRemove &&
               _lastLogType != RocksDBLogType::SingleRemove) {
      // collection drops etc may be batched directly after a transaction
      // single operation updates could come in a weird sequence pre 3.3:
      // [..., LogType::SinglePut, DELETE old, PUT new, ...]
      if (_lastLogType != RocksDBLogType::SinglePut) {
        resetTransientState(); // finish ongoing trx
      }
      return rocksdb::Status();
    }
    TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
    TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
    TRI_ASSERT(!_removeDocumentKey.empty());

    LocalDocumentId docId = RocksDBKey::documentId(key);
    _builder.openObject();
    _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
    _builder.add("type", VPackValue(static_cast<uint64_t>(REPLICATION_MARKER_REMOVE)));
    _builder.add("database", VPackValue(std::to_string(_currentDbId)));
    _builder.add("cid", VPackValue(std::to_string(_currentCid)));
    std::string const& cname = nameFromCid(_currentCid);
    if (!cname.empty()) {
      _builder.add("cname", VPackValue(cname));
    }
    _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
    _builder.add("data", VPackValue(VPackValueType::Object));
    _builder.add(StaticStrings::KeyString, VPackValue(_removeDocumentKey));
    _builder.add(StaticStrings::RevString, VPackValue(TRI_RidToString(docId.id())));
    _builder.close();
    _builder.close();
    _removeDocumentKey.clear();
    if (_singleOp) { // reset state immediately
      resetTransientState();
    }
    return rocksdb::Status();
  }

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _lastLogType = RocksDBLogType::Invalid;
    _startOfBatch = true;
  }

  void writeCommitMarker() {
    TRI_ASSERT(_seenBeginTransaction && !_singleOp);
    LOG_TOPIC(_LOG, Logger::ROCKSDB) << "tick: " << _currentSequence
                                    << " commit transaction";

    _builder.openObject();
    _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
    _builder.add("type",
        VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_COMMIT)));
    _builder.add("database", VPackValue(std::to_string(_currentDbId)));
    _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
    _builder.close();
  }
  
  // should reset state flags which are only valid between
  // observing a specific log entry and a sequence of immediately
  // following PUT / DELETE / Log entries
  void resetTransientState() {
    if (_seenBeginTransaction) {
      writeCommitMarker();
    }
    // reset all states
    _lastLogType = RocksDBLogType::Invalid;
    _seenBeginTransaction = false;
    _singleOp = false;
    _currentTrxId = 0;
    _currentDbId = 0;
    _currentCid = 0;
    _removeDocumentKey.clear();
    _oldCollectionName.clear();
  }

  uint64_t endBatch() {
    TRI_ASSERT(_removeDocumentKey.empty());
    resetTransientState();
    return _currentSequence;
  }

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
  
  bool shouldHandleDB(TRI_voc_tick_t dbid) const {
    return _vocbase->id() == dbid;
  }
  
  /// @brief Check if collection is in filter
  bool shouldHandleCollection(TRI_voc_tick_t dbid,
                              TRI_voc_cid_t cid) const {
    return shouldHandleDB(dbid) &&
           (_onlyCollectionId == 0 || _onlyCollectionId == cid);
  }

  bool shouldHandleKey(uint32_t column_family_id,
                       bool isPut, rocksdb::Slice const& key) const {
    TRI_voc_tick_t dbId = 0;
    TRI_voc_cid_t cid = 0;
    if (column_family_id == _definitionsCF) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        return false;// ignore in this protocol version
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
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
        TRI_ASSERT(dbId == 0 && cid == 0);
        return false;
      }
    } else {
      return false;
    }

    // only return results for one collection
    if (!shouldHandleCollection(dbId, cid)) {
      return false;
    }

    // allow document removes of dropped collections
    std::string const collectionName = _vocbase->collectionName(cid);
    if (collectionName.empty()) {
      return true;
    }
    if (!_includeSystem && collectionName[0] == '_') {
      return false;
    }
    if (TRI_ExcludeCollectionReplication(collectionName, _includeSystem)) {
      return false;
    }

    return true;
  }

  /// @brief translate a (local) collection id into a collection name
  std::string const& nameFromCid(TRI_voc_cid_t cid) {
    auto it = _collectionNames.find(cid);

    if (it != _collectionNames.end()) {
      // collection name is in cache already
      return (*it).second;
    }

    // collection name not in cache yet
    std::string name(_vocbase->collectionName(cid));

    if (!name.empty()) {
      // insert into cache
      try {
        _collectionNames.emplace(cid, std::move(name));
      } catch (...) {
        return emptyString;
      }

      // and look it up again
      return nameFromCid(cid);
    }

    return emptyString;
  }

 private:
  uint32_t const _documentsCF;
  uint32_t const _definitionsCF;
  
  // these parameters are relevant to determine if we can print
  // a specific marker from the WAL
  TRI_vocbase_t* const _vocbase;
  
  bool const _includeSystem;
  TRI_voc_cid_t const _onlyCollectionId;
  /// result builder
  VPackBuilder& _builder;
  // collection name cache
  std::unordered_map<TRI_voc_cid_t, std::string> _collectionNames;

  // Various state machine flags
  rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;
  RocksDBLogType _lastLogType = RocksDBLogType::Invalid;
  bool _seenBeginTransaction = false;
  bool _singleOp = false;
  bool _startOfBatch = false;
  TRI_voc_tick_t _currentTrxId = 0;
  TRI_voc_tick_t _currentDbId = 0;
  TRI_voc_cid_t _currentCid = 0;
  std::string _oldCollectionName;
  std::string _removeDocumentKey;
};

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database
RocksDBReplicationResult rocksutils::tailWal(TRI_vocbase_t* vocbase,
                                             uint64_t tickStart,
                                             uint64_t tickEnd, size_t chunkSize,
                                             bool includeSystem,
                                             TRI_voc_cid_t collectionId,
                                             VPackBuilder& builder) {
  TRI_ASSERT(tickStart <= tickEnd);
  uint64_t lastTick = tickStart;// generally contains begin of last wb
  uint64_t lastWrittenTick = tickStart;// contains end tick of last wb
  uint64_t lastScannedTick = tickStart;
  
  //LOG_TOPIC(WARN, Logger::FIXME) << "1. Starting tailing: tickStart " <<
  //tickStart << " tickEnd " << tickEnd << " chunkSize " << chunkSize;//*/

  std::unique_ptr<WALParser> handler(
      new WALParser(vocbase, includeSystem, collectionId, builder));
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;

  rocksdb::Status s;
  // no need verifying the WAL contents
  rocksdb::TransactionLogIterator::ReadOptions ro(false);
  uint64_t since = std::max(tickStart - 1, (uint64_t)0);
  s = rocksutils::globalRocksDB()->GetUpdatesSince(since, &iterator, ro);
  
  if (!s.ok()) {
    auto converted = convertStatus(s, rocksutils::StatusHint::wal);
    return {converted.errorNumber(), lastTick};
  }

  bool minTickIncluded = false;
  // we need to check if the builder is bigger than the chunksize,
  // only after we printed a full WriteBatch. Otherwise a client might
  // never read the full writebatch
  while (iterator->Valid() && lastTick <= tickEnd &&
         builder.buffer()->size() < chunkSize) {
    s = iterator->status();
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan";
      LOG_TOPIC(ERR, Logger::ROCKSDB) << s.ToString();
      break; // s is considered in the end
    }
    
    rocksdb::BatchResult batch = iterator->GetBatch();
    TRI_ASSERT(lastTick == tickStart || batch.sequence >= lastTick);

    if (batch.sequence <= tickEnd) {
      lastScannedTick = batch.sequence;
    }

    if (!minTickIncluded && batch.sequence <= tickStart &&
        batch.sequence <= tickEnd) {
      minTickIncluded = true;
    }
    if (batch.sequence <= tickStart) {
      iterator->Next(); // skip
      continue;
    } else if (batch.sequence > tickEnd) {
      break; // cancel out
    }

    lastTick = batch.sequence;
    LOG_TOPIC(_LOG, Logger::ROCKSDB) << "Start WriteBatch tick: " << lastTick;
    handler->startNewBatch(batch.sequence);
    s = batch.writeBatchPtr->Iterate(handler.get());
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan";
      LOG_TOPIC(ERR, Logger::ROCKSDB) << s.ToString();
      break; // s is considered in the end
    }
    
    lastWrittenTick = handler->endBatch();
    LOG_TOPIC(_LOG, Logger::ROCKSDB) << "End WriteBatch written-tick: "
                                     << lastWrittenTick;
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
  
  // LOG_TOPIC(WARN, Logger::FIXME) << "2.  lastWrittenTick: " << lastWrittenTick;
  return result;
}
