////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBEngine/RocksDBWalTailer.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Replication/TailingSyncer.h"
#include "RestServer/DatabaseFeature.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBCommon.h"
#include "RocksDBEngine/RocksDBLogValue.h"
#include "RocksDBEngine/RocksDBReplicationTailing.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalView.h"

#include "Logger/Logger.h"
#include "Logger/LogMacros.h"

#include <rocksdb/utilities/transaction_db.h>
#include <velocypack/Builder.h>

using namespace arangodb;

namespace {

class RocksDBTailingWALDumper final : public rocksdb::WriteBatch::Handler {
 public:
  RocksDBTailingWALDumper(
      RocksDBEngine& engine,
      std::function<void(RocksDBWalTailer::Marker const&)> const& callback)
      : _engine(engine),
        _definitionsCF(RocksDBColumnFamilyManager::get(
                           RocksDBColumnFamilyManager::Family::Definitions)
                           ->GetID()),
        _documentsCF(RocksDBColumnFamilyManager::get(
                         RocksDBColumnFamilyManager::Family::Documents)
                         ->GetID()),
        _primaryCF(RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::PrimaryIndex)
                       ->GetID()),
        _currentSequence(0),
        _callback(callback) {}

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    // TODO: needed?
    _currentSequence++;

    if (column_family_id == _documentsCF) {
      auto objectId = RocksDBKey::objectId(key);
      auto [_, datasourceId] = _engine.mapObjectToCollection(objectId);

      _callback(
          RocksDBWalTailer::PutMarker{.tick = _currentSequence,
                                      .datasourceId = datasourceId,
                                      .documentId = RocksDBKey::documentId(key),
                                      // TODO: lifetime?
                                      .document = RocksDBValue::data(value)});
    }
    return rocksdb::Status();
  }

  // for Delete / SingleDelete
  void handleDeleteCF(uint32_t cfId, rocksdb::Slice const& key) {
    _currentSequence++;

    if (cfId != _primaryCF) {
      return;  // ignore all document operations
    }

    auto objectId = RocksDBKey::objectId(key);
    auto [_, datasourceId] = _engine.mapObjectToCollection(objectId);

    _callback(RocksDBWalTailer::DeleteMarker{
        .datasourceId = datasourceId,
        .documentId = RocksDBKey::documentId(key)});
  }

  rocksdb::Status DeleteCF(uint32_t column_family_id,
                           rocksdb::Slice const& key) override {
    handleDeleteCF(column_family_id, key);
    return rocksdb::Status();
  }

  rocksdb::Status SingleDeleteCF(uint32_t column_family_id,
                                 rocksdb::Slice const& key) override {
    handleDeleteCF(column_family_id, key);
    return rocksdb::Status();
  }

  rocksdb::Status DeleteRangeCF(uint32_t column_family_id,
                                const rocksdb::Slice& begin_key,
                                const rocksdb::Slice& end_key) override {
    _currentSequence++;
    return rocksdb::Status();
  }

  rocksdb::Status MarkNoop(bool /*empty_batch*/) override {
    return rocksdb::Status::OK();
  }

 private:
  RocksDBEngine& _engine;
  uint32_t const _definitionsCF;
  uint32_t const _documentsCF;
  uint32_t const _primaryCF;

  // rocksdb::SequenceNumber _startSequence;
  rocksdb::SequenceNumber _currentSequence;
  //  rocksdb::SequenceNumber _lastWrittenSequence;

  std::function<void(RocksDBWalTailer::Marker const&)> const& _callback;
};

}  // namespace

void RocksDBWalTailer::tail(
    std::function<void(Marker const&)> const& func) const {
  rocksdb::TransactionDB* db = _engine.db();

  RocksDBTailingWALDumper dumper(_engine, func);
  const uint64_t since = _startTick;

  uint64_t firstTick = UINT64_MAX;   // first tick to actually print (exclusive)
  uint64_t lastScannedTick = since;  // last (begin) tick of batch we looked at
  uint64_t latestTick = db->GetLatestSequenceNumber();

  RocksDBFilePurgePreventer purgePreventer(_engine.disallowPurging());

  std::unique_ptr<rocksdb::TransactionLogIterator> iterator;
  rocksdb::TransactionLogIterator::ReadOptions ro(false);
  rocksdb::Status s = db->GetUpdatesSince(since, &iterator, ro);
  if (!s.ok()) {
    ADB_PROD_ASSERT(false) << "TODO: opening RocksDB iterator";
  }

  while (iterator->Valid() && lastScannedTick <= _endTick) {
    rocksdb::BatchResult batch = iterator->GetBatch();
    // record the first tick we are actually considering
    if (firstTick == UINT64_MAX) {
      firstTick = batch.sequence;
    }

    if (batch.sequence > _endTick) {
      break;
    }
    lastScannedTick = batch.sequence;

    if (batch.sequence < since) {
      iterator->Next();
      continue;
    }

    s = batch.writeBatchPtr->Iterate(&dumper);

    if (!s.ok()) {
      LOG_TOPIC("57d54", ERR, Logger::REPLICATION)
          << "error during WAL scan: " << s.ToString();
      //  break;
    }

    lastScannedTick = batch.sequence;
    TRI_ASSERT(lastScannedTick <= db->GetLatestSequenceNumber());

    iterator->Next();
  }

  latestTick = db->GetLatestSequenceNumber();
  TRI_ASSERT(lastScannedTick <= latestTick);

  // ADB_PROD_ASSERT(s.ok()) << "TODO: error handling";
}
