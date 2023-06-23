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
      RocksDBEngine& engine, rocksdb::SequenceNumber tick,
      std::function<void(RocksDBWalTailer::Marker const&)> const& callback)
      : _engine(engine),
        _documentsCF(RocksDBColumnFamilyManager::get(
                         RocksDBColumnFamilyManager::Family::Documents)
                         ->GetID()),
        _primaryCF(RocksDBColumnFamilyManager::get(
                       RocksDBColumnFamilyManager::Family::PrimaryIndex)
                       ->GetID()),
        _tick(tick),
        _callback(callback) {}

  rocksdb::Status PutCF(uint32_t column_family_id, rocksdb::Slice const& key,
                        rocksdb::Slice const& value) override {
    if (column_family_id == _documentsCF) {
      auto objectId = RocksDBKey::objectId(key);
      auto [_, datasourceId] = _engine.mapObjectToCollection(objectId);

      _callback(
          RocksDBWalTailer::PutMarker{.tick = _tick,
                                      .datasourceId = datasourceId,
                                      .documentId = RocksDBKey::documentId(key),
                                      // TODO: lifetime?
                                      .document = RocksDBValue::data(value)});
    }
    return rocksdb::Status();
  }

  // for Delete / SingleDelete
  void handleDeleteCF(uint32_t cfId, rocksdb::Slice const& key) {
    if (cfId != _primaryCF) {
      return;  // ignore all document operations
    }

    auto objectId = RocksDBKey::objectId(key);
    auto [_, datasourceId] = _engine.mapObjectToCollection(objectId);

    _callback(RocksDBWalTailer::DeleteMarker{
        .tick = _tick,
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
    return rocksdb::Status();
  }

  rocksdb::Status MarkNoop(bool /*empty_batch*/) override {
    return rocksdb::Status::OK();
  }

 private:
  RocksDBEngine& _engine;
  uint32_t const _documentsCF;
  uint32_t const _primaryCF;

  rocksdb::SequenceNumber _tick;

  std::function<void(RocksDBWalTailer::Marker const&)> const& _callback;
};

}  // namespace

auto RocksDBWalTailer::tail(
    std::function<void(Marker const&)> const& func) const -> TailingResult {
  auto* db = _engine.db();
  auto const since = _startTick;

  auto purgePreventer = RocksDBFilePurgePreventer(_engine.disallowPurging());

  auto iterator = std::unique_ptr<rocksdb::TransactionLogIterator>(nullptr);
  auto readOptions = rocksdb::TransactionLogIterator::ReadOptions(false);

  auto status = db->GetUpdatesSince(since, &iterator, readOptions);
  if (!status.ok()) {
    return TailingResult::error(status);
  }

  while (iterator->Valid()) {
    auto batch = iterator->GetBatch();
    if (batch.sequence > _endTick) {
      break;
    }

    if (batch.sequence < since) {
      iterator->Next();
      continue;
    }

    auto dumper = RocksDBTailingWALDumper(_engine, batch.sequence, func);
    auto iterateStatus = batch.writeBatchPtr->Iterate(&dumper);

    if (!iterateStatus.ok()) {
      return TailingResult::error(iterateStatus);
    }
    iterator->Next();

    if (batch.sequence > _endTick) {
      break;
    }
  }
  return TailingResult::ok();
}
