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
///
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "LogStorageMethods.h"

#include <Basics/Result.h>
#include <Futures/Future.h>

#include "Replication2/ReplicatedLog/LogEntries.h"
#include "Replication2/Storage/RocksDB/LogPersistor.h"
#include "Replication2/Storage/RocksDB/StatePersistor.h"

#include <utility>

using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

namespace arangodb::replication2::storage::rocksdb {

LogStorageMethods::LogStorageMethods(
    uint64_t objectId, std::uint64_t vocbaseId, replication2::LogId logId,
    std::shared_ptr<IAsyncLogWriteBatcher> batcher, ::rocksdb::DB* db,
    ::rocksdb::ColumnFamilyHandle* metaCf, ::rocksdb::ColumnFamilyHandle* logCf,
    std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics)
    : ctx(vocbaseId, objectId),
      _logPersistor(std::make_unique<LogPersistor>(
          logId, ctx, db, logCf, std::move(batcher), std::move(metrics))),
      _statePersistor(
          std::make_unique<StatePersistor>(logId, ctx, db, metaCf)) {}

Result LogStorageMethods::updateMetadata(storage::PersistedStateInfo info) {
  return _statePersistor->updateMetadata(info);
}

ResultT<PersistedStateInfo> LogStorageMethods::readMetadata() {
  return _statePersistor->readMetadata();
}

std::unique_ptr<PersistedLogIterator> LogStorageMethods::read(LogIndex first) {
  return _logPersistor->read(first);
}

auto LogStorageMethods::removeFront(LogIndex stop, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->removeFront(stop, opts);
}

auto LogStorageMethods::removeBack(LogIndex start, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->removeBack(start, opts);
}

auto LogStorageMethods::insert(std::unique_ptr<PersistedLogIterator> iter,
                               WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return _logPersistor->insert(std::move(iter), opts);
}

uint64_t LogStorageMethods::getObjectId() {
  return _logPersistor->getObjectId();
}
LogId LogStorageMethods::getLogId() { return _logPersistor->getLogId(); }

auto LogStorageMethods::getSyncedSequenceNumber() -> SequenceNumber {
  return _logPersistor->getSyncedSequenceNumber();
}

auto LogStorageMethods::waitForSync(
    IStorageEngineMethods::SequenceNumber number) -> futures::Future<Result> {
  return _logPersistor->waitForSync(number);
}

void LogStorageMethods::waitForCompletion() noexcept {
  _logPersistor->waitForCompletion();
}

auto LogStorageMethods::drop() -> Result {
  // TODO - can we make this atomic?
  // Probably rather tricky once we write our own WAL files
  auto res = _statePersistor->drop();
  if (res.ok()) {
    res = _logPersistor->drop();
  }
  return res;
}

auto LogStorageMethods::compact() -> Result { return _logPersistor->compact(); }

}  // namespace arangodb::replication2::storage::rocksdb
