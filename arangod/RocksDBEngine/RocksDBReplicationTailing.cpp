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
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "VocBase/replication-common.h"
#include "VocBase/ticks.h"

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

// iterates over WAL starting at 'from' and returns up to 'limit' documents
// from the corresponding database
RocksDBReplicationResult rocksutils::tailWal(TRI_vocbase_t* vocbase,
                                             uint64_t from, size_t limit,
                                             bool includeSystem,
                                             VPackBuilder& builder) {
  uint64_t lastTick = from;
  std::unique_ptr<WBReader> handler(
      new WBReader(vocbase, from, limit, includeSystem, builder));
  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;  // reader();
  rocksdb::Status s = static_cast<rocksdb::DB*>(globalRocksDB())
                          ->GetUpdatesSince(from, &iterator);
  if (!s.ok()) {  // TODO do something?
    auto converted = convertStatus(s);
    return {converted.errorNumber(), lastTick};
  }

  while (iterator->Valid() && limit > 0) {
    s = iterator->status();
    if (s.ok()) {
      rocksdb::BatchResult batch = iterator->GetBatch();
      lastTick = batch.sequence;
      s = batch.writeBatchPtr->Iterate(handler.get());
    }
    if (!s.ok()) {
      LOG_TOPIC(ERR, Logger::ENGINES) << "Error during WAL scan";
      LOG_TOPIC(ERR, Logger::ENGINES) << iterator->status().getState();
      auto converted = convertStatus(s);
      return {converted.errorNumber(), lastTick};
    }

    iterator->Next();
  }

  return {TRI_ERROR_NO_ERROR, lastTick};
}
