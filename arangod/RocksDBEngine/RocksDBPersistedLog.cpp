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

#include <Basics/Exceptions.h>
#include <Basics/RocksDBUtils.h>
#include <Basics/ScopeGuard.h>
#include <Basics/debugging.h>
#include <Basics/application-exit.h>
#include <Logger/LogMacros.h>
#include <Basics/ResultT.h>
#include <Inspection/VPack.h>
#include <memory>
#include "Metrics/Gauge.h"
#include "Metrics/LogScale.h"
#include "Metrics/Histogram.h"

#include <utility>
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RocksDBKey.h"
#include "RocksDBPersistedLog.h"
#include "RocksDBValue.h"
#include "RocksDBKeyBounds.h"
#include "Replication2/MetricsHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::replicated_state;

void AsyncLogWriteContext::waitForCompletion() {
  while (true) {
    auto c = _pendingAsyncOperations.load(std::memory_order_acquire);
    if (c == 0) {
      break;
    }
    _pendingAsyncOperations.wait(c, std::memory_order_acquire);
  }
}

void AsyncLogWriteContext::finishPendingAsyncOperation() {
  auto c = _pendingAsyncOperations.fetch_sub(1, std::memory_order_release);
  if (c == 1) {
    _pendingAsyncOperations.notify_all();
  }
}

void AsyncLogWriteContext::addPendingAsyncOperation() {
  _pendingAsyncOperations.fetch_add(1, std::memory_order_release);
}

struct RocksDBLogIterator : PersistedLogIterator {
  ~RocksDBLogIterator() override = default;

  RocksDBLogIterator(std::uint64_t objectId, rocksdb::DB* db,
                     rocksdb::ColumnFamilyHandle* cf, LogIndex start)
      : _bounds(RocksDBKeyBounds::LogRange(objectId)),
        _upperBound(_bounds.end()) {
    rocksdb::ReadOptions opts;
    opts.prefix_same_as_start = true;
    opts.iterate_upper_bound = &_upperBound;
    _iter.reset(db->NewIterator(opts, cf));

    auto first = RocksDBKey{};
    first.constructLogEntry(objectId, start);
    _iter->Seek(first.string());
  }

  auto next() -> std::optional<PersistingLogEntry> override {
    if (!_first) {
      _iter->Next();
    }
    _first = false;

    if (!_iter->Valid()) {
      auto s = _iter->status();
      if (!s.ok()) {
        auto res = rocksutils::convertStatus(s);
        THROW_ARANGO_EXCEPTION_MESSAGE(res.errorNumber(), res.errorMessage());
      }

      return std::nullopt;
    }

    return std::optional<PersistingLogEntry>{
        std::in_place, RocksDBKey::logIndex(_iter->key()),
        RocksDBValue::data(_iter->value())};
  }

  RocksDBKeyBounds const _bounds;
  rocksdb::Slice const _upperBound;
  std::unique_ptr<rocksdb::Iterator> _iter;
  bool _first = true;
};

RocksDBAsyncLogWriteBatcher::RocksDBAsyncLogWriteBatcher(
    rocksdb::ColumnFamilyHandle* cf, rocksdb::DB* db,
    std::shared_ptr<IAsyncExecutor> executor,
    std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> options,
    std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> metrics)
    : _cf(cf),
      _db(db),
      _executor(std::move(executor)),
      _options(std::move(options)),
      _metrics(std::move(metrics)) {
  _lanes[0]._numWorkerMetrics = _metrics->numWorkerThreadsWaitForSync;
  _lanes[1]._numWorkerMetrics = _metrics->numWorkerThreadsNoWaitForSync;
}

void RocksDBAsyncLogWriteBatcher::runPersistorWorker(Lane& lane) noexcept {
  // This function is noexcept so in case an exception bubbles up we
  // rather crash instead of losing one thread.
  GaugeScopedCounter metricsCounter(*lane._numWorkerMetrics);
  while (true) {
    std::vector<Request> pendingRequests;
    {
      // std::mutex::lock is not noexcept, but there is no other solution
      // to this problem instead of crashing the server.
      std::unique_lock guard(lane._persistorMutex);
      if (lane._pendingPersistRequests.empty()) {
        // no more work to do
        lane._activePersistorThreads -= 1;
        return;
      }
      std::swap(pendingRequests, lane._pendingPersistRequests);
      _metrics->queueLength->operator-=(pendingRequests.size());
    }

    auto nextReqToWrite = pendingRequests.begin();
    auto nextReqToResolve = nextReqToWrite;
    // We sort the logs by their ids. This will make the write batch sorted
    // in ascending order, which should improve performance in RocksDB.
    // Remember, the keys for the individual log entries are constructed as
    // <8-byte big endian log id> <8-byte big endian index>
    std::sort(pendingRequests.begin(), pendingRequests.end(),
              [](Request const& left, Request const& right) {
                return left.objectId < right.objectId;
              });

    auto result = basics::catchToResult([&] {
      rocksdb::WriteBatch wb;

      while (nextReqToWrite != std::end(pendingRequests)) {
        wb.Clear();

        // For simplicity, a single LogIterator of a specific PersistRequest
        // (i.e. *nextReqToWrite->iter) is always written as a whole in a write
        // batch. This is not strictly necessary for correctness, as long as an
        // error is reported when any PersistingLogEntry is not written. Because
        // then, the write will be retried, and it does not hurt that the
        // persisted log already has some entries that are not yet confirmed
        // (which may be overwritten later). This could still be improved upon a
        // little by reporting up to which entry was written successfully.
        while (wb.GetDataSize() < _options->_thresholdRocksDBWriteBatchSize &&
               nextReqToWrite != std::end(pendingRequests)) {
          if (auto res = prepareRequest(*nextReqToWrite, wb); res.fail()) {
            return res;
          }
          ++nextReqToWrite;
        }
        {
          _metrics->writeBatchSize->count(wb.GetDataSize());
          {
            MeasureTimeGuard metricsGuard(*_metrics->rocksdbWriteTimeInUs);
            if (auto s = _db->Write({}, &wb); !s.ok()) {
              return rocksutils::convertStatus(s);
            }
          }
          if (lane._waitForSync) {
            {
              MeasureTimeGuard metricsGuard(*_metrics->rocksdbSyncTimeInUs);
              if (auto s = _db->SyncWAL(); !s.ok()) {
                // At this point we have to make sure that every previous log
                // entry is synced as well. Otherwise we might get holes in the
                // log.
                return rocksutils::convertStatus(s);
              }
            }
          }
        }

        // Promise used used to signal that data has been synced to disk up to
        // the last sequence number.
        futures::Promise<Result> syncedToDisk;
        auto seq = _db->GetLatestSequenceNumber();

        // resolve all promises in [nextReqToResolve, nextReqToWrite)
        for (; nextReqToResolve != nextReqToWrite; ++nextReqToResolve) {
          _executor->operator()(
              [reqToResolve = std::move(*nextReqToResolve),
               syncedToDiskFuture =
                   syncedToDisk.getFuture()]() mutable noexcept {
                reqToResolve.promise.setValue(
                    ResultT{std::move(syncedToDiskFuture)});
              });
        }

        _waitForSyncPromises.doUnderLock([&](auto& promises) {
          auto [_, inserted] = promises.emplace(seq, std::move(syncedToDisk));
          TRI_ASSERT(inserted) << "Duplicate sequence number " << seq
                               << " in waitForSyncPromises";
        });
      }

      return Result{TRI_ERROR_NO_ERROR};
    });

    // resolve all promises with the result
    if (result.fail()) {
      for (; nextReqToResolve != std::end(pendingRequests);
           ++nextReqToResolve) {
        // If a promise is fulfilled before (with a value), nextReqToResolve
        // should always be increased as well; meaning we only exactly iterate
        // over the unfulfilled promises here.
        TRI_ASSERT(!nextReqToResolve->promise.isFulfilled());
        _executor->operator()([reqToResolve = std::move(*nextReqToResolve),
                               result = result]() mutable noexcept {
          TRI_ASSERT(!reqToResolve.promise.isFulfilled());
          reqToResolve.promise.setValue(std::move(result));
        });
      }
    }
  }
}

auto RocksDBAsyncLogWriteBatcher::queue(AsyncLogWriteContext& ctx,
                                        Action action,
                                        WriteOptions const& options)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  auto const startNewThread = [&](Lane& lane) {
    size_t num_retries = 0;
    while (true) {
      auto lambda = fu2::unique_function<void() noexcept>{
          [self = shared_from_this(), &lane]() noexcept {
            self->runPersistorWorker(lane);
          }};

      try {
        _executor->operator()(
            std::move(lambda));  // may throw a QUEUE_FULL exception
        break;
      } catch (std::exception const& e) {
        LOG_TOPIC("213cb", WARN, Logger::REPLICATION2)
            << "Could not post persistence request onto the scheduler: "
            << e.what() << " Retries: " << num_retries;
      } catch (...) {
        LOG_TOPIC("8553d", WARN, Logger::REPLICATION2)
            << "Could not post persistence request onto the scheduler."
            << " Retries: " << num_retries;
      }

      using namespace std::chrono_literals;
      std::this_thread::sleep_for(
          100us * (1 << std::min(num_retries, static_cast<size_t>(15))));
      num_retries += 1;
    }
  };

  auto queueRequest = [&](Lane& lane)
      -> std::pair<futures::Future<ResultT<futures::Future<Result>>>, bool> {
    std::unique_lock guard(lane._persistorMutex);
    auto& req =
        lane._pendingPersistRequests.emplace_back(ctx, std::move(action));
    bool const wantNewThread = lane._activePersistorThreads == 0 ||
                               (lane._pendingPersistRequests.size() > 100 &&
                                lane._activePersistorThreads < 2);
    if (wantNewThread) {
      lane._activePersistorThreads += 1;
    }
    _metrics->queueLength->operator++();
    return std::make_pair(req.promise.getFuture(), wantNewThread);
  };

  // pick a lane based on the options
  Lane& lane = _lanes[options.waitForSync ? 0 : 1];
  TRI_ASSERT(lane._waitForSync == options.waitForSync);

  auto [future, wantNewThread] = queueRequest(lane);
  if (wantNewThread) {
    startNewThread(lane);
  }
  return std::move(future);
}

auto RocksDBAsyncLogWriteBatcher::prepareRequest(Request const& req,
                                                 rocksdb::WriteBatch& wb)
    -> Result {
  return std::visit(
      overload{[&](InsertEntries const& what) {
                 auto key = RocksDBKey{};
                 rocksdb::Status s;
                 while (auto entry = what.iter->next()) {
                   key.constructLogEntry(req.objectId, entry->logIndex());
                   auto value = RocksDBValue::LogEntry(*entry);
                   s = wb.Put(_cf, key.string(), value.string());
                   if (!s.ok()) {
                     break;
                   }
                 }
                 return rocksutils::convertStatus(s);
               },
               [&](RemoveFront const& what) {
                 auto bounds = RocksDBKeyBounds::LogRange(req.objectId);
                 auto last = RocksDBKey();
                 last.constructLogEntry(req.objectId, what.stop);
                 auto s = wb.DeleteRange(_cf, bounds.start(), last.string());
                 return rocksutils::convertStatus(s);
               },
               [&](RemoveBack const& what) {
                 auto bounds = RocksDBKeyBounds::LogRange(req.objectId);
                 auto first = RocksDBKey();
                 first.constructLogEntry(req.objectId, what.start);
                 auto s = wb.DeleteRange(_cf, first.string(), bounds.end());
                 return rocksutils::convertStatus(s);
               }},
      req.action);
}

auto RocksDBAsyncLogWriteBatcher::queueInsert(
    AsyncLogWriteContext& ctx,
    std::unique_ptr<replication2::PersistedLogIterator> iter,
    WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  return queue(ctx, InsertEntries{.iter = std::move(iter)}, opts);
}

auto RocksDBAsyncLogWriteBatcher::queueRemoveFront(AsyncLogWriteContext& ctx,
                                                   replication2::LogIndex stop,
                                                   WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  return queue(ctx, RemoveFront{.stop = stop}, opts);
}

auto RocksDBAsyncLogWriteBatcher::queueRemoveBack(AsyncLogWriteContext& ctx,
                                                  replication2::LogIndex start,
                                                  WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  return queue(ctx, RemoveBack{.start = start}, opts);
}

void RocksDBAsyncLogWriteBatcher::onSync(
    SequenceNumber sequenceNumber) noexcept {
  try {
    // Will post a request on the scheduler
    _executor->operator()(
        [self = shared_from_this(), sequenceNumber]() noexcept {
          std::vector<futures::Promise<Result>> promises;

          auto guard = self->_waitForSyncPromises.getLockedGuard();
          auto end = guard->upper_bound(sequenceNumber);
          for (auto it = guard->begin(); it != end; ++it) {
            promises.emplace_back(std::move(it->second));
          }
          guard->erase(guard->begin(), end);
          guard.unlock();

          for (auto& promise : promises) {
            promise.setValue(Result{});
          }
        });
  } catch (std::exception const& e) {
    LOG_TOPIC("282be", FATAL, Logger::REPLICATION2)
        << "Could not schedule an update after syncing log entries to disk: "
        << e.what() << " Sequence number: " << sequenceNumber;
  } catch (...) {
    LOG_TOPIC("5572a", FATAL, Logger::REPLICATION2)
        << "Could not schedule an update after syncing log entries to disk."
        << " Sequence number: " << sequenceNumber;
  }
}

Result RocksDBLogStorageMethods::updateMetadata(
    replicated_state::PersistedStateInfo info) {
  TRI_ASSERT(info.stateId == logId);  // redundant information

  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);

  RocksDBReplicatedStateInfo rInfo;
  rInfo.dataSourceId = logId.id();
  rInfo.stateId = logId;
  rInfo.objectId = ctx.objectId;
  rInfo.state = std::move(info);

  VPackBuilder valueBuilder;
  velocypack::serialize(valueBuilder, rInfo);
  auto value = RocksDBValue::ReplicatedState(valueBuilder.slice());

  rocksdb::WriteOptions opts;
  auto s = db->GetRootDB()->Put(opts, metaCf, key.string(), value.string());

  return rocksutils::convertStatus(s);
}

ResultT<PersistedStateInfo> RocksDBLogStorageMethods::readMetadata() {
  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);

  std::string value;
  auto s = db->GetRootDB()->Get(rocksdb::ReadOptions{}, metaCf, key.string(),
                                &value);
  if (!s.ok()) {
    return rocksutils::convertStatus(s);
  }

  auto slice = VPackSlice(reinterpret_cast<uint8_t const*>(value.data()));
  auto info = velocypack::deserialize<RocksDBReplicatedStateInfo>(slice);

  TRI_ASSERT(info.stateId == logId);
  return std::move(info.state);
}

std::unique_ptr<PersistedLogIterator> RocksDBLogStorageMethods::read(
    LogIndex first) {
  return std::make_unique<RocksDBLogIterator>(ctx.objectId, db, logCf, first);
}

auto RocksDBLogStorageMethods::removeFront(LogIndex stop,
                                           WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  IRocksDBAsyncLogWriteBatcher::WriteOptions wo;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyRemoveFront);
  wo.waitForSync = opts.waitForSync;
  return batcher->queueRemoveFront(ctx, stop, wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

auto RocksDBLogStorageMethods::removeBack(LogIndex start,
                                          WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  IRocksDBAsyncLogWriteBatcher::WriteOptions wo;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyRemoveBack);
  wo.waitForSync = opts.waitForSync;
  return batcher->queueRemoveBack(ctx, start, wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

auto RocksDBLogStorageMethods::insert(
    std::unique_ptr<PersistedLogIterator> iter, WriteOptions const& opts)
    -> futures::Future<ResultT<futures::Future<Result>>> {
  IRocksDBAsyncLogWriteBatcher::WriteOptions wo;
  wo.waitForSync = opts.waitForSync;
  MeasureTimeGuard timeGuard(*_metrics->operationLatencyInsert);
  return batcher->queueInsert(ctx, std::move(iter), wo)
      .then([timeGuard = std::move(timeGuard)](auto&& tryResult) mutable {
        timeGuard.fire();
        return std::move(tryResult).get();
      });
}

uint64_t RocksDBLogStorageMethods::getObjectId() { return ctx.objectId; }
LogId RocksDBLogStorageMethods::getLogId() { return logId; }

RocksDBLogStorageMethods::RocksDBLogStorageMethods(
    uint64_t objectId, std::uint64_t vocbaseId, replication2::LogId logId,
    std::shared_ptr<IRocksDBAsyncLogWriteBatcher> persistor, rocksdb::DB* db,
    rocksdb::ColumnFamilyHandle* metaCf, rocksdb::ColumnFamilyHandle* logCf,
    std::shared_ptr<RocksDBAsyncLogWriteBatcherMetrics> metrics)
    : logId(logId),
      batcher(std::move(persistor)),
      db(db),
      metaCf(metaCf),
      logCf(logCf),
      ctx(vocbaseId, objectId),
      _metrics(std::move(metrics)) {}

auto RocksDBLogStorageMethods::getSyncedSequenceNumber() -> SequenceNumber {
  FATAL_ERROR_ABORT();  // TODO not implemented
}

auto RocksDBLogStorageMethods::waitForSync(
    IStorageEngineMethods::SequenceNumber number)
    -> futures::Future<futures::Unit> {
  FATAL_ERROR_ABORT();  // TODO not implemented
}

auto RocksDBLogStorageMethods::drop() -> Result {
  // prepare deletion transaction
  rocksdb::WriteBatch batch;
  auto key = RocksDBKey{};
  key.constructReplicatedState(ctx.vocbaseId, logId);
  if (auto s = batch.Delete(metaCf, key.string()); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  auto range = RocksDBKeyBounds::LogRange(ctx.objectId);
  // TODO should we remove using rocksutils::removeLargeRange instead?
  auto start = range.start();
  auto end = range.end();
  if (auto s = batch.DeleteRange(logCf, start, end); !s.ok()) {
    return rocksutils::convertStatus(s);
  }
  if (auto s = db->GetRootDB()->Write(rocksdb::WriteOptions{}, &batch);
      !s.ok()) {
    return rocksutils::convertStatus(s);
  }
  return {};
}

auto RocksDBLogStorageMethods::compact() -> Result {
  auto range = RocksDBKeyBounds::LogRange(ctx.objectId);
  auto start = range.start();
  auto end = range.end();
  auto res = db->CompactRange(
      rocksdb::CompactRangeOptions{.exclusive_manual_compaction = false,
                                   .allow_write_stall = false},
      logCf, &start, &end);
  return rocksutils::convertStatus(res);
}

void RocksDBLogStorageMethods::waitForCompletion() noexcept {
  ctx.waitForCompletion();
}
