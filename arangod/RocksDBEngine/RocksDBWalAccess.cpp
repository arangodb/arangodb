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
  rocksutils::globalRocksEngine()->syncWal();
  return rocksutils::globalRocksDB()->GetLatestSequenceNumber();
}

/// should return the list of transactions started, but not committed in that
/// range (range can be adjusted)
WalAccessResult RocksDBWalAccess::openTransactions(
    uint64_t tickStart, uint64_t tickEnd, WalAccess::Filter const& filter,
    TransactionCallback const&) const {
  return WalAccessResult(TRI_ERROR_NO_ERROR, true, 0, 0);
}

/// WAL parser
class MyWALParser : public rocksdb::WriteBatch::Handler,
                    public WalAccessContext {
 public:
  MyWALParser(WalAccess::Filter const& filter,
              WalAccess::MarkerCallback const& f)
      : WalAccessContext(filter, f),
        _documentsCF(RocksDBColumnFamily::documents()->GetID()),
        _definitionsCF(RocksDBColumnFamily::definitions()->GetID()),
        _startSequence(0),
        _currentSequence(0) {}

  void LogData(rocksdb::Slice const& blob) override {
    tick();
    RocksDBLogType type = RocksDBLogValue::type(blob);
    TRI_DEFER(_lastLogType = type);

    // LOG_TOPIC(ERR, Logger::FIXME) << "[LOG] " << rocksDBLogTypeName(type);
    switch (type) {
      case RocksDBLogType::DatabaseCreate: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        // wait for marker data in Put entry
        break;
      }
      case RocksDBLogType::DatabaseDrop: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        std::string name = RocksDBLogValue::databaseName(blob).toString();
        {
          VPackObjectBuilder marker(&_builder, true);
          marker->add("tick", VPackValue(std::to_string(_currentSequence)));
          marker->add("type", VPackValue(rocksutils::convertLogType(type)));
          marker->add("db", VPackValue(name));
        }
        _callback(loadVocbase(_currentDbId), _builder.slice());
        _builder.clear();
        break;
      }
      case RocksDBLogType::CollectionRename:
      case RocksDBLogType::CollectionCreate:
      case RocksDBLogType::CollectionChange: {
        if (_lastLogType == RocksDBLogType::IndexCreate) {
          TRI_ASSERT(_currentDbId == RocksDBLogValue::databaseId(blob));
          TRI_ASSERT(_currentCid == RocksDBLogValue::collectionId(blob));
        }
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        break;
      }
      case RocksDBLogType::CollectionDrop: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        StringRef uuid = RocksDBLogValue::collectionUUID(blob);
        TRI_ASSERT(!uuid.empty());

        TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
        if (vocbase != nullptr) {
          {
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(_currentSequence)));
            marker->add("type", VPackValue(rocksutils::convertLogType(type)));
            marker->add("db", VPackValue(vocbase->name()));
            marker->add("cuid", VPackValuePair(uuid.data(), uuid.size(),
                                               VPackValueType::String));
          }
          _callback(loadVocbase(_currentDbId), _builder.slice());
          _builder.clear();
        }
        break;
      }
      case RocksDBLogType::IndexCreate: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        // only print markers from this collection if it is set
        if (shouldHandleCollection(_currentDbId, _currentCid)) {
          LogicalCollection* col = loadCollection(_currentDbId, _currentCid);
          if (col != nullptr) {
            {
              VPackObjectBuilder marker(&_builder, true);
              marker->add("type", VPackValue(rocksutils::convertLogType(type)));
              marker->add("db", VPackValue(loadVocbase(_currentDbId)->name()));
              marker->add("cuid", VPackValue(col->globallyUniqueId()));
              marker->add("data", RocksDBLogValue::indexSlice(blob));
            }
            _callback(loadVocbase(_currentDbId), _builder.slice());
            _builder.clear();
          }
        }
        break;
      }
      case RocksDBLogType::IndexDrop: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
        // only print markers from this collection if it is set
        if (shouldHandleCollection(_currentDbId, _currentCid)) {
          TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
          LogicalCollection* col = loadCollection(_currentDbId, _currentCid);
          if (vocbase != nullptr && col != nullptr) {
            {
              VPackObjectBuilder marker(&_builder, true);
              marker->add("type", VPackValue(rocksutils::convertLogType(type)));
              marker->add("db", VPackValue(vocbase->name()));
              marker->add("cuid", VPackValue(col->globallyUniqueId()));
              VPackObjectBuilder data(&_builder, "data", true);
              data->add("id", VPackValue(std::to_string(iid)));
            }
            _callback(vocbase, _builder.slice());
            _builder.clear();
          }
        }
        break;
      }
      case RocksDBLogType::ViewCreate:
      case RocksDBLogType::ViewChange:
      case RocksDBLogType::ViewDrop: {
        // TODO
        break;
      }
      case RocksDBLogType::BeginTransaction: {
        TRI_ASSERT(!_singleOp);
        _seenBeginTransaction = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentTrxId = RocksDBLogValue::transactionId(blob);
        TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
        if (vocbase != nullptr) {
          {
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(_currentSequence)));
            marker->add("type", VPackValue(rocksutils::convertLogType(type)));
            marker->add("db", VPackValue(vocbase->name()));
            marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
          }
          _callback(vocbase, _builder.slice());
          _builder.clear();
        }

        break;
      }
      case RocksDBLogType::DocumentOperationsPrologue: {
        _currentCid = RocksDBLogValue::collectionId(blob);
        break;
      }
      case RocksDBLogType::DocumentRemove: {
        _removeDocumentKey = RocksDBLogValue::documentKey(blob).toString();
        break;
      }
      case RocksDBLogType::SingleRemove: {
        _removeDocumentKey = RocksDBLogValue::documentKey(blob).toString();
        // intentional fall through
      }
      case RocksDBLogType::SinglePut: {
        writeCommitMarker();
        _singleOp = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCid = RocksDBLogValue::collectionId(blob);
        _currentTrxId = 0;
        break;
      }

      default:
        LOG_TOPIC(WARN, Logger::REPLICATION) << "Unhandled wal log entry "
                                             << rocksDBLogTypeName(type);
        break;
    }
  }

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    tick();
    if (!shouldHandleMarker(column_family_id, key)) {
      return rocksdb::Status();
    }
    // LOG_TOPIC(ERR, Logger::ROCKSDB) << "[PUT] cf: " << column_family_id << ",
    // key:" << key.ToString()
    //<< "  value: " << value.ToString();

    if (column_family_id == _definitionsCF) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        TRI_ASSERT(_lastLogType == RocksDBLogType::DatabaseCreate ||
                   _lastLogType == RocksDBLogType::DatabaseDrop);
        if (_lastLogType == RocksDBLogType::DatabaseCreate) {
          TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
          if (vocbase != nullptr) {
            {
              VPackObjectBuilder marker(&_builder, true);
              marker->add("tick", VPackValue(std::to_string(_currentSequence)));
              marker->add("type",
                          VPackValue(rocksutils::convertLogType(_lastLogType)));
              marker->add("db", VPackValue(vocbase->name()));
              marker->add("data", RocksDBValue::data(value));
            }
            _callback(loadVocbase(_currentDbId), _builder.slice());
            _builder.clear();
          }
        }
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        // creating indexes will change collection entry in rocksdb
        // we do not transfer this sperately
        if (_lastLogType == RocksDBLogType::IndexCreate ||
            _lastLogType == RocksDBLogType::IndexDrop) {
          _lastLogType = RocksDBLogType::Invalid;
          return rocksdb::Status();
        }

        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionCreate ||
                   _lastLogType == RocksDBLogType::CollectionChange ||
                   _lastLogType == RocksDBLogType::CollectionRename);
        TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);

        TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
        LogicalCollection* col = loadCollection(_currentDbId, _currentCid);
        if (vocbase != nullptr && col != nullptr) {
          {
            VPackObjectBuilder marker(&_builder, true);
            marker->add("tick", VPackValue(std::to_string(_currentSequence)));
            marker->add("type",
                        VPackValue(rocksutils::convertLogType(_lastLogType)));
            marker->add("db", VPackValue(loadVocbase(_currentDbId)->name()));
            marker->add("cuid", VPackValue(col->globallyUniqueId()));
            if (_lastLogType == RocksDBLogType::CollectionRename) {
              VPackObjectBuilder data(&_builder, "data", true);
              data->add("name", VPackValue(col->name()));
            } else {  // change and create need full data
              marker->add("data", RocksDBValue::data(value));
            }
          }
          _callback(loadVocbase(_currentDbId), _builder.slice());
          _builder.clear();
        }

        // log type is only ever relevant, immediately after it appeared
        // we want double occurences create / drop / change collection to fail
        _lastLogType = RocksDBLogType::Invalid;
        _currentDbId = 0;
        _currentCid = 0;
      }  // if (RocksDBKey::type(key) == RocksDBEntryType::Collection)

    } else if (column_family_id == _documentsCF) {
      TRI_ASSERT((_seenBeginTransaction && !_singleOp) ||
                 (!_seenBeginTransaction && _singleOp));
      // if real transaction, we need the trx id
      TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
      TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);

      TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
      LogicalCollection* col = loadCollection(_currentDbId, _currentCid);
      if (vocbase != nullptr && col != nullptr) {
        {
          VPackObjectBuilder marker(&_builder, true);
          marker->add("tick", VPackValue(std::to_string(_currentSequence)));
          marker->add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
          marker->add("db", VPackValue(vocbase->name()));
          marker->add("cuid", VPackValue(col->globallyUniqueId()));
          if (_singleOp) {  // single op is defined to have a transaction id of
                            // 0
            marker->add("tid", VPackValue("0"));
            _singleOp = false;
          } else {
            marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
          }
          marker->add("data", RocksDBValue::data(value));
        }
        _callback(loadVocbase(_currentDbId), _builder.slice());
        _builder.clear();
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
    if (!shouldHandleMarker(column_family_id, key)) {
      return rocksdb::Status();
    }
    // LOG_TOPIC(ERR, Logger::ROCKSDB) << "[Delete] cf: " << column_family_id <<
    // " key:" << key.ToString();

    if (column_family_id == _definitionsCF) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        // we already print this marker upon reading the log marker.
        // databases are deleted when the last reference to it disappears
        // not when _dropDatabase is called
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionDrop);
        TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
        // we already printed this marker upon reading the log value
      }
    } else if (column_family_id == _documentsCF) {
      // document removes, because of a drop is not transactional and
      // should not appear in the WAL. Allso fixes
      if (!(_seenBeginTransaction || _singleOp)) {
        return rocksdb::Status();
      }
      // FIXME: why does the previous if not make this obsolete
      if (_lastLogType != RocksDBLogType::DocumentRemove &&
          _lastLogType != RocksDBLogType::SingleRemove) {
        return rocksdb::Status();
      }
      // TRI_ASSERT(_lastLogType == RocksDBLogType::DocumentRemove ||
      //           _lastLogType == RocksDBLogType::SingleRemove);
      TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
      TRI_ASSERT(_currentDbId != 0 && _currentCid != 0);
      TRI_ASSERT(!_removeDocumentKey.empty());

      TRI_vocbase_t* vocbase = loadVocbase(_currentDbId);
      LogicalCollection* col = loadCollection(_currentDbId, _currentCid);
      if (vocbase != nullptr && col != nullptr) {
        uint64_t revId =
            RocksDBKey::revisionId(RocksDBEntryType::Document, key);
        {
          VPackObjectBuilder marker(&_builder, true);
          marker->add("tick", VPackValue(std::to_string(_currentSequence)));
          marker->add("type", VPackValue(REPLICATION_MARKER_REMOVE));
          marker->add("db", VPackValue(vocbase->name()));
          marker->add("cuid", VPackValue(col->globallyUniqueId()));
          if (_singleOp) {  // single op is defined to 0
            marker->add("tid", VPackValue("0"));
            _singleOp = false;
          } else {
            marker->add("tid", VPackValue(std::to_string(_currentTrxId)));
          }
          VPackObjectBuilder data(&_builder, "data", true);
          data->add(StaticStrings::KeyString, VPackValue(_removeDocumentKey));
          data->add(StaticStrings::RevString,
                    VPackValue(std::to_string(revId)));
        }
        _callback(loadVocbase(_currentDbId), _builder.slice());
        _builder.clear();
        _removeDocumentKey.clear();
      }
    }
    return rocksdb::Status();
  }

  void startNewBatch(rocksdb::SequenceNumber startSequence) {
    // starting new write batch
    _startSequence = startSequence;
    _currentSequence = startSequence;
    _startOfBatch = true;
  }

  void writeCommitMarker() {
    if (_seenBeginTransaction) {
      _builder.openObject();
      _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
      _builder.add(
          "type",
          VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_COMMIT)));
      _builder.add("db", VPackValue(loadVocbase(_currentDbId)->name()));
      _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
      _builder.close();
      _callback(loadVocbase(_currentDbId), _builder.slice());
      _builder.clear();
    }
    // reset all states
    _lastLogType = RocksDBLogType::Invalid;
    _seenBeginTransaction = false;
    _singleOp = false;
    _startOfBatch = true;
    _currentDbId = 0;
    _currentTrxId = 0;
    _currentCid = 0;
    //_removeDocumentKey.clear(); can not remove here
    _indexSlice = VPackSlice::illegalSlice();
  }

  uint64_t endBatch() {
    writeCommitMarker();
    _removeDocumentKey.clear();
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

  bool shouldHandleMarker(uint32_t column_family_id,
                          rocksdb::Slice const& key) {
    TRI_voc_tick_t dbId = 0;
    TRI_voc_cid_t cid = 0;
    if (column_family_id == _definitionsCF) {
      if (RocksDBKey::type(key) == RocksDBEntryType::Database) {
        return _filter.vocbase == 0;
      } else if (RocksDBKey::type(key) == RocksDBEntryType::Collection) {
        //  || RocksDBKey::type(key) == RocksDBEntryType::View
        dbId = RocksDBKey::databaseId(key);
        cid = RocksDBKey::collectionId(key);
        if (dbId == 0 || cid == 0) {
          // FIXME: I don't get why this happens, seems broken
          // to get a key with zero entries here
          return false;
        }
      } else {
        return false;
      }
    } else if (column_family_id == _documentsCF) {
      dbId = _currentDbId;
      cid = _currentCid;
      // happens when dropping a collection
      if (!(_seenBeginTransaction || _singleOp)) {
        TRI_ASSERT(dbId == 0 && cid == 0);
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
  }

 private:
  uint32_t const _documentsCF;
  uint32_t const _definitionsCF;

  rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;

  // Various state machine flags
  RocksDBLogType _lastLogType = RocksDBLogType::Invalid;
  bool _seenBeginTransaction = false;
  bool _singleOp = false;
  bool _startOfBatch = false;
  TRI_voc_tick_t _currentDbId = 0;
  TRI_voc_tick_t _currentTrxId = 0;
  TRI_voc_cid_t _currentCid = 0;
  std::string _removeDocumentKey;
  VPackSlice _indexSlice;
};

// iterates over WAL starting at 'from' and returns up to 'chunkSize' documents
// from the corresponding database
WalAccessResult RocksDBWalAccess::tail(uint64_t tickStart, uint64_t tickEnd,
                                       size_t chunkSize,
                                       TRI_voc_tick_t barrierId,
                                       Filter const& filter,
                                       MarkerCallback const& func) const {
  TRI_ASSERT(filter.transactionIds.empty());  // not supported in any way

  // we have to start scanning at tickStart - 1 here because GetUpdatesSince
  // will
  // exclude the specified tick value. we however want to check whether the
  // provided
  // tick value is still somewhere present in the RocksDB WAL
  if (tickStart > 0) {
    tickStart = tickStart - 1;
  }
  // LOG_TOPIC(ERR, Logger::FIXME) << "1. Starting tailing: tickStart " <<
  // tickStart
  //<< " tickEnd " << tickEnd << " chunkSize " << chunkSize << " includeSystem "
  //<< includeSystem;
  rocksdb::TransactionDB* db = rocksutils::globalRocksDB();

  uint64_t firstTick = UINT64_MAX;  // first tick actually read
  uint64_t lastTick = tickStart;    // lastTick at start of a write batch
  uint64_t lastWrittenTick = 0;     // lastTick at the end of a write batch
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
                           0, latestTick);
  }

  // we need to check if the builder is bigger than the chunksize,
  // only after we printed a full WriteBatch. Otherwise a client might
  // never read the full writebatch
  while (iterator->Valid() && lastTick <= tickEnd &&
         handler->responseSize() < chunkSize) {
    s = iterator->status();
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan: "
                                      << s.ToString();
      break;  // s is considered in the end
    }

    rocksdb::BatchResult batch = iterator->GetBatch();
    // record the first tick we are actually considering
    if (firstTick == UINT64_MAX) {
      firstTick = batch.sequence;
    }

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
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan: "
                                      << s.ToString();
      break;  // s is considered in the end
    }
    lastWrittenTick = handler->endBatch();  // end of the batch

    TRI_ASSERT(lastWrittenTick >= lastTick);
    iterator->Next();
  }

  WalAccessResult result(TRI_ERROR_NO_ERROR, firstTick <= tickStart,
                         lastWrittenTick, latestTick);
  if (!s.ok()) {
    result.Result::reset(convertStatus(s, rocksutils::StatusHint::wal));
  }
  // LOG_TOPIC(ERR, Logger::FIXME) << "2. firstTick " << firstTick << "
  // lastWrittenTick " << lastWrittenTick;
  return result;
}
