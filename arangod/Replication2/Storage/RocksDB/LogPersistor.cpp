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

#include "LogPersistor.h"

#include <Futures/Future.h>

#include "Basics/application-exit.h"
#include "Basics/RocksDBUtils.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteBatcherMetrics.h"
#include "Replication2/Storage/RocksDB/IAsyncLogWriteBatcher.h"
#include "Replication2/Storage/RocksDB/LogIterator.h"

namespace arangodb::replication2::storage::rocksdb {

LogPersistor::LogPersistor(LogId logId, uint64_t objectId,
                           std::uint64_t vocbaseId, ::rocksdb::DB* const db,
                           ::rocksdb::ColumnFamilyHandle* const logCf,
                           std::shared_ptr<IAsyncLogWriteBatcher> batcher,
                           std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics)
    : logId(logId),
      ctx(vocbaseId, objectId),
      batcher(std::move(batcher)),
      _metrics(std::move(metrics)),
      db(db),
      logCf(logCf) {}

std::unique_ptr<replication2::LogIterator> LogPersistor::getIterator(
    IteratorPosition position) {
  return std::make_unique<LogIterator>(ctx.objectId, db, logCf,
                                       position.index());
}

auto LogPersistor::removeFront(LogIndex stop, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  IAsyncLogWriteBatcher::WriteOptions wo;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyRemoveFront);
  wo.waitForSync = opts.waitForSync;
  return batcher->queueRemoveFront(ctx, stop, wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

auto LogPersistor::removeBack(LogIndex start, WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  IAsyncLogWriteBatcher::WriteOptions wo;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyRemoveBack);
  wo.waitForSync = opts.waitForSync;
  return batcher->queueRemoveBack(ctx, start, wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

auto LogPersistor::insert(std::unique_ptr<replication2::LogIterator> iter,
                          WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  IAsyncLogWriteBatcher::WriteOptions wo;
  wo.waitForSync = opts.waitForSync;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyInsert);
  return batcher->queueInsert(ctx, std::move(iter), wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

uint64_t LogPersistor::getObjectId() { return ctx.objectId; }
LogId LogPersistor::getLogId() { return logId; }

auto LogPersistor::getSyncedSequenceNumber() -> SequenceNumber {
  FATAL_ERROR_ABORT();  // TODO not implemented
}

auto LogPersistor::waitForSync(IStorageEngineMethods::SequenceNumber number)
    -> futures::Future<Result> {
  return batcher->waitForSync(number);
}

void LogPersistor::waitForCompletion() noexcept { ctx.waitForCompletion(); }

auto LogPersistor::drop() -> Result {
  auto range = RocksDBKeyBounds::LogRange(ctx.objectId);
  // TODO should we remove using rocksutils::removeLargeRange instead?
  auto start = range.start();
  auto end = range.end();
  auto s = db->GetRootDB()->DeleteRange(::rocksdb::WriteOptions{}, logCf, start,
                                        end);
  return rocksutils::convertStatus(s);
}

auto LogPersistor::compact() -> Result {
  auto range = RocksDBKeyBounds::LogRange(ctx.objectId);
  auto start = range.start();
  auto end = range.end();
  auto res = db->CompactRange(
      ::rocksdb::CompactRangeOptions{.exclusive_manual_compaction = false,
                                     .allow_write_stall = false},
      logCf, &start, &end);
  return rocksutils::convertStatus(res);
}

}  // namespace arangodb::replication2::storage::rocksdb
