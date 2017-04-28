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

#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringRef.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "VocBase/replication-common.h"
#include "VocBase/ticks.h"

#include <rocksdb/utilities/transaction_db.h>
#include <rocksdb/utilities/write_batch_with_index.h>
#include <rocksdb/write_batch.h>

using namespace arangodb;
using namespace arangodb::rocksutils;
using namespace arangodb::velocypack;

/// an incomplete convert function, basically only use for DDL ops
static TRI_replication_operation_e convertLogType(RocksDBLogType t) {
  switch (t) {
    //    case Rocks:
    //      return REPLICATION_MARKER_DOCUMENT;
    //    case TRI_DF_MARKER_VPACK_REMOVE:
    //      return REPLICATION_MARKER_REMOVE;
    //  case RocksDBLogType::BeginTransaction:
    //  return REPLICATION_TRANSACTION_START;
    //    case TRI_DF_MARKER_VPACK_COMMIT_TRANSACTION:
    //      return REPLICATION_TRANSACTION_COMMIT;
    //    case TRI_DF_MARKER_VPACK_ABORT_TRANSACTION:
    //      return REPLICATION_TRANSACTION_ABORT;
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

    default:
      return REPLICATION_INVALID;
  }
}

/// WAL parser
class WALParser : public rocksdb::WriteBatch::Handler {
 public:
  explicit WALParser(TRI_vocbase_t* vocbase, uint64_t from, size_t& limit,
                     bool includeSystem, VPackBuilder& builder)
  : _vocbase(vocbase),
  _from(from),
  _limit(limit),
  _includeSystem(includeSystem),
  _builder(builder) {}

  void LogData(rocksdb::Slice const& blob) override {
    if (_currentSequence < _from) {
      return;
    }
    
    RocksDBLogType type = RocksDBLogValue::type(blob);
    TRI_DEFER(_lastLogType = type);
    switch (type) {
      case RocksDBLogType::DatabaseCreate: {
        // FIXME: do we have to print something?
        break;
      }
      case RocksDBLogType::DatabaseDrop: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        // FIXME: do we have to print something?
        break;
      }
      case RocksDBLogType::CollectionRename: {
        _oldCollectionName =
            RocksDBLogValue::newCollectionName(blob).toString();
        // intentional fallthrough
      }
      case RocksDBLogType::CollectionCreate:
      case RocksDBLogType::CollectionChange:
      case RocksDBLogType::CollectionDrop: {
        if (_lastLogType == RocksDBLogType::IndexCreate) {
          TRI_ASSERT(_currentDbId == RocksDBLogValue::databaseId(blob));
          TRI_ASSERT(_currentCollectionId ==
                     RocksDBLogValue::collectionId(blob));
        }
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCollectionId = RocksDBLogValue::collectionId(blob);
        break;
      }
      case RocksDBLogType::IndexCreate: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCollectionId = RocksDBLogValue::collectionId(blob);
        VPackSlice indexSlice = RocksDBLogValue::indexSlice(blob);
        _builder.openObject();
        _builder.add(
            "type",
            VPackValue(static_cast<uint64_t>(REPLICATION_INDEX_CREATE)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));
        _builder.add("data", indexSlice);
        _builder.close();
        break;
      }
      case RocksDBLogType::IndexDrop: {
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCollectionId = RocksDBLogValue::collectionId(blob);
        TRI_idx_iid_t iid = RocksDBLogValue::indexId(blob);
        _builder.openObject();
        _builder.add(
            "type",
            VPackValue(static_cast<uint64_t>(REPLICATION_INDEX_DROP)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));
        _builder.add("data", VPackValue(VPackValueType::Object));
        _builder.add("id", VPackValue(std::to_string(iid)));
        _builder.close();
        _builder.close();
        break;
      }
      case RocksDBLogType::ViewCreate: {
        // TODO
        break;
      }
      case RocksDBLogType::ViewDrop: {
        // TODO
        break;
      }
      case RocksDBLogType::ViewChange: {
        // TODO
        break;
      }
      case RocksDBLogType::BeginTransaction: {
        _seenBeginTransaction = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentTrxId = RocksDBLogValue::transactionId(blob);

        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add(
            "type",
            VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_START)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
        _builder.close();
        break;
      }
      case RocksDBLogType::DocumentOperationsPrologue: {
        _currentCollectionId = RocksDBLogValue::collectionId(blob);
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
        _singleOpTransaction = true;
        _currentDbId = RocksDBLogValue::databaseId(blob);
        _currentCollectionId = RocksDBLogValue::collectionId(blob);
        _currentTrxId = 0;
        break;
      }

      default:
        break;
    }
  }

  void Put(rocksdb::Slice const& key, rocksdb::Slice const& value) override {
    if (!shouldHandleKey(key) || _currentSequence < _from) {
      return;
    }
    switch (RocksDBKey::type(key)) {
      case RocksDBEntryType::Collection: {
        if (_lastLogType == RocksDBLogType::IndexCreate ||
            _lastLogType == RocksDBLogType::IndexDrop) {
          return;
        }
        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionCreate ||
                   _lastLogType == RocksDBLogType::CollectionChange ||
                   _lastLogType == RocksDBLogType::CollectionRename);
        TRI_ASSERT(_currentDbId != 0 && _currentCollectionId != 0);
        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add("type", VPackValue(convertLogType(_lastLogType)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));

        if (_lastLogType == RocksDBLogType::CollectionRename) {
          VPackSlice collectionData(value.data());
          _builder.add("data", VPackValue(VPackValueType::Object));
          _builder.add("id", VPackValue(std::to_string(_currentCollectionId)));
          _builder.add("oldName", VPackValue(_oldCollectionName));
          _builder.add("name", collectionData.get("name"));
          _builder.close();
        } else {  // change and create need full data
          _builder.add("data", RocksDBValue::data(value));
        }
        _builder.close();
        break;
      }
      case RocksDBEntryType::Document: {
        TRI_ASSERT(_seenBeginTransaction || _singleOpTransaction);
        TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
        TRI_ASSERT(_currentDbId != 0 && _currentCollectionId != 0);

        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add("type", VPackValue(REPLICATION_MARKER_DOCUMENT));
        // auto containers = getContainerIds(key);
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));
        if (_singleOpTransaction) {  // single op is defined to 0
          _builder.add("tid", VPackValue("0"));
        } else {
          _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
        }
        _builder.add("data", RocksDBValue::data(value));
        _builder.close();

        break;
      }
      default:
        break;  // shouldn't get here?
    }

    if (_limit > 0) {
      _limit--;
    }
  }

  void Delete(rocksdb::Slice const& key) override { handleDeletion(key); }

  void SingleDelete(rocksdb::Slice const& key) override { handleDeletion(key); }

  void handleDeletion(rocksdb::Slice const& key) {
    if (_currentSequence < _from) {
      return;
    }
    
    switch (RocksDBKey::type(key)) {
      case RocksDBEntryType::Collection: {
        TRI_ASSERT(_lastLogType == RocksDBLogType::CollectionDrop);
        TRI_ASSERT(_currentDbId != 0 && _currentCollectionId != 0);

        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add("type", VPackValue(REPLICATION_COLLECTION_DROP));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));
        _builder.add("data", VPackValue(VPackValueType::Object));
        _builder.add("id", VPackValue(std::to_string(_currentCollectionId)));
        _builder.add("name", VPackValue(""));  // not used at all
        _builder.close();
        _builder.close();
        break;
      }
      case RocksDBEntryType::Document: {
        // document removes, because of a drop is not transactional and
        // should not appear in the WAL
        if (!shouldHandleKey(key) ||
            !(_seenBeginTransaction || _singleOpTransaction)) {
          return;
        }

        TRI_ASSERT(!_seenBeginTransaction || _currentTrxId != 0);
        TRI_ASSERT(_currentDbId != 0 && _currentCollectionId != 0);
        TRI_ASSERT(!_removeDocumentKey.empty());
        
        uint64_t revisionId = RocksDBKey::revisionId(key);
        _builder.openObject();
        _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
        _builder.add(
                     "type",
                     VPackValue(static_cast<uint64_t>(REPLICATION_MARKER_REMOVE)));
        _builder.add("database", VPackValue(std::to_string(_currentDbId)));
        _builder.add("cid", VPackValue(std::to_string(_currentCollectionId)));
        if (_singleOpTransaction) {  // single op is defined to 0
          _builder.add("tid", VPackValue("0"));
        } else {
          _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
        }
        _builder.add("data", VPackValue(VPackValueType::Object));
        _builder.add(StaticStrings::KeyString, VPackValue(_removeDocumentKey));
        _builder.add(StaticStrings::RevString,
                     VPackValue(std::to_string(revisionId)));
        _builder.close();
        _builder.close();
        break;
      }
      default:
        break;  // shouldn't get here?
    }
    
    if (_limit > 0) {
      _limit--;
    }
  }

  void startNewBatch(rocksdb::SequenceNumber currentSequence) {
    // starting new write batch
    _currentSequence = currentSequence;
    _lastLogType = RocksDBLogType::Invalid;
    _seenBeginTransaction = false;
    _singleOpTransaction = false;
    _currentDbId = 0;
    _currentTrxId = 0;
    _currentCollectionId = 0;
    _oldCollectionName.clear();
    _removeDocumentKey.clear();
  }

  void endBatch() {
    if (_seenBeginTransaction) {
      _builder.openObject();
      _builder.add("tick", VPackValue(std::to_string(_currentSequence)));
      _builder.add(
          "type",
          VPackValue(static_cast<uint64_t>(REPLICATION_TRANSACTION_COMMIT)));
      _builder.add("database", VPackValue(std::to_string(_currentDbId)));
      _builder.add("tid", VPackValue(std::to_string(_currentTrxId)));
      _builder.close();
    }
    _seenBeginTransaction = false;
    _singleOpTransaction = false;
  }

 private:
  bool shouldHandleKey(rocksdb::Slice const& key) {
    if (_limit == 0) {
      return false;
    } else if (_includeSystem) {
      return true;
    }

    TRI_voc_cid_t cid;
    switch (RocksDBKey::type(key)) {
      case RocksDBEntryType::Collection: {
        cid = RocksDBKey::collectionId(key);
        break;
      }
      case RocksDBEntryType::Document: {
        uint64_t objectId = RocksDBKey::objectId(key);
        auto mapping = mapObjectToCollection(objectId);
        if (mapping.first != _vocbase->id()) {
          return false;
        }
        cid = mapping.second;
      }
      case RocksDBEntryType::View:  // should handle these eventually?
      default:
        return false;
    }

    // allow document removes of dropped collections
    std::string const collectionName = _vocbase->collectionName(cid);
    if (collectionName.size() == 0) {
      return true;
    }
    if (!_includeSystem && collectionName[0] == '_') {
      return false;
    }

    return true;
  }

 private:
  TRI_vocbase_t* _vocbase;
  uint64_t _from;
  size_t& _limit;
  bool _includeSystem;
  VPackBuilder& _builder;

  rocksdb::SequenceNumber _currentSequence;
  RocksDBLogType _lastLogType = RocksDBLogType::Invalid;
  bool _seenBeginTransaction = false;
  bool _singleOpTransaction = false;
  TRI_voc_tick_t _currentDbId = 0;
  TRI_voc_tick_t _currentTrxId = 0;
  TRI_voc_cid_t _currentCollectionId = 0;
  std::string _oldCollectionName;
  std::string _removeDocumentKey;
  VPackSlice _indexSlice;
};

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database
RocksDBReplicationResult rocksutils::tailWal(TRI_vocbase_t* vocbase,
                                             uint64_t from, size_t limit,
                                             bool includeSystem,
                                             VPackBuilder& builder) {

  uint64_t lastTick = from;
  std::unique_ptr<WALParser> handler(
      new WALParser(vocbase, from, limit, includeSystem, builder));
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = static_cast<rocksdb::DB*>(globalRocksDB())
                          ->GetUpdatesSince(from, &iterator);
  if (!s.ok()) {  // TODO do something?
    auto converted = convertStatus(s);
    return {converted.errorNumber(), lastTick};
  }

  bool fromTickIncluded = false;
  while (iterator->Valid() && limit > 0) {
    s = iterator->status();
    if (s.ok()) {
      rocksdb::BatchResult batch = iterator->GetBatch();
      lastTick = batch.sequence;
      if (lastTick == from) {
        fromTickIncluded = true;
      }
      handler->startNewBatch(batch.sequence);
      s = batch.writeBatchPtr->Iterate(handler.get());
      if (s.ok()) {
        handler->endBatch();
      }
    } else {
      LOG_TOPIC(ERR, Logger::ENGINES) << "error during WAL scan";
      auto converted = convertStatus(s);
      auto result = RocksDBReplicationResult(converted.errorNumber(), lastTick);
      if (fromTickIncluded) {
        result.includeFromTick();
      }
      return result;
    }

    iterator->Next();
  }

  auto result = RocksDBReplicationResult(TRI_ERROR_NO_ERROR, lastTick);
  if (fromTickIncluded) {
    result.includeFromTick();
  }

  return result;
}
