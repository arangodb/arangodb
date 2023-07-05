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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "AsyncLogWriteBatcher.h"

#include "Basics/RocksDBUtils.h"
#include "Logger/LogMacros.h"
#include "Metrics/Gauge.h"
#include "Metrics/Histogram.h"
#include "Metrics/LogScale.h"
#include "Replication2/MetricsHelper.h"
#include "Replication2/ReplicatedLog/LogEntry.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteContext.h"
#include "Replication2/Storage/RocksDB/AsyncLogWriteBatcherMetrics.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBValue.h"

#include <rocksdb/db.h>

namespace arangodb::replication2::storage::rocksdb {

AsyncLogOperationGuard::AsyncLogOperationGuard(AsyncLogWriteContext& ctx)
    : _context(&ctx) {
  ctx.addPendingAsyncOperation();
}

AsyncLogOperationGuard::~AsyncLogOperationGuard() { fire(); }

void AsyncLogOperationGuard::fire() noexcept {
  if (_context) {
    _context->finishPendingAsyncOperation();
    _context = nullptr;
  }
}

AsyncLogWriteBatcher::AsyncLogWriteBatcher(
    ::rocksdb::ColumnFamilyHandle* cf, ::rocksdb::DB* db,
    std::shared_ptr<IAsyncExecutor> executor,
    std::shared_ptr<replication2::ReplicatedLogGlobalSettings const> options,
    std::shared_ptr<AsyncLogWriteBatcherMetrics> metrics)
    : _cf(cf),
      _db(db),
      _executor(std::move(executor)),
      _options(std::move(options)),
      _metrics(std::move(metrics)) {
  _lanes[0]._numWorkerMetrics = _metrics->numWorkerThreadsWaitForSync;
  _lanes[1]._numWorkerMetrics = _metrics->numWorkerThreadsNoWaitForSync;
}

AsyncLogWriteBatcher::~AsyncLogWriteBatcher() {
  // We need to make sure that all pending requests are finished before we are
  // destroyed. This should not happen normally, but if it does, we'll handle it
  // gracefully.
  auto& guard = _syncGuard.getLockedGuard().get();
  if (!guard.promises.empty()) {
    LOG_TOPIC("5f6f9", WARN, Logger::REPLICATION2)
        << guard.promises.size()
        << " wait-for-sync promises remaining in RocksDBAsyncLogWriteBatcher "
           "destructor, the last known synced sequence number is "
        << guard.syncedSequenceNumber;
    for (auto&& [_, promise] : guard.promises) {
      promise.setValue(Result{TRI_ERROR_SHUTTING_DOWN});
    }
  }
}

void AsyncLogWriteBatcher::runPersistorWorker(Lane& lane) noexcept {
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
      ::rocksdb::WriteBatch wb;

      while (nextReqToWrite != std::end(pendingRequests)) {
        wb.Clear();

        // For simplicity, a single LogIterator of a specific PersistRequest
        // (i.e. *nextReqToWrite->iter) is always written as a whole in a write
        // batch. This is not strictly necessary for correctness, as long as an
        // error is reported when any LogEntry is not written. Because
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

        auto seq = _db->GetLatestSequenceNumber();

        // resolve all promises in [nextReqToResolve, nextReqToWrite)
        for (; nextReqToResolve != nextReqToWrite; ++nextReqToResolve) {
          _executor->operator()(
              [seq,
               reqToResolve = std::move(*nextReqToResolve)]() mutable noexcept {
                reqToResolve.promise.setValue(ResultT{seq});
              });
        }
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

auto AsyncLogWriteBatcher::queue(AsyncLogWriteContext& ctx, Action action,
                                 WriteOptions const& options)
    -> futures::Future<ResultT<SequenceNumber>> {
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
      -> std::pair<futures::Future<ResultT<SequenceNumber>>, bool> {
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

auto AsyncLogWriteBatcher::prepareRequest(Request const& req,
                                          ::rocksdb::WriteBatch& wb) -> Result {
  return std::visit(
      overload{[&](InsertEntries const& what) {
                 auto key = RocksDBKey{};
                 ::rocksdb::Status s;
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

auto AsyncLogWriteBatcher::queueInsert(
    AsyncLogWriteContext& ctx, std::unique_ptr<replication2::LogIterator> iter,
    WriteOptions const& opts) -> futures::Future<ResultT<SequenceNumber>> {
  return queue(ctx, InsertEntries{.iter = std::move(iter)}, opts);
}

auto AsyncLogWriteBatcher::queueRemoveFront(AsyncLogWriteContext& ctx,
                                            replication2::LogIndex stop,
                                            WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return queue(ctx, RemoveFront{.stop = stop}, opts);
}

auto AsyncLogWriteBatcher::queueRemoveBack(AsyncLogWriteContext& ctx,
                                           replication2::LogIndex start,
                                           WriteOptions const& opts)
    -> futures::Future<ResultT<SequenceNumber>> {
  return queue(ctx, RemoveBack{.start = start}, opts);
}

auto AsyncLogWriteBatcher::waitForSync(SequenceNumber seq)
    -> futures::Future<Result> {
  return _syncGuard.doUnderLock([&](auto& guard) {
    auto promise = futures::Promise<Result>{};
    auto fut = promise.getFuture();
    if (seq <= guard.syncedSequenceNumber) {
      promise.setValue(Result{});
    } else {
      guard.promises.emplace(seq, std::move(promise));
    }
    return fut;
  });
}

void AsyncLogWriteBatcher::onSync(SequenceNumber seq) noexcept {
  // Schedule a task to notify all the futures waiting for the sequence number
  // to be synced.
  auto res = basics::catchVoidToResult([&]() {
    _executor->operator()([self = shared_from_this(), seq]() noexcept {
      std::vector<futures::Promise<Result>> promises;

      self->_syncGuard.doUnderLock([&](auto& guard) {
        if (seq < guard.syncedSequenceNumber) {
          return;
        }
        guard.syncedSequenceNumber = seq;

        auto end = guard.promises.upper_bound(seq);
        for (auto it = guard.promises.begin(); it != end; ++it) {
          promises.emplace_back(std::move(it->second));
        }
        guard.promises.erase(guard.promises.begin(), end);
      });

      for (auto& promise : promises) {
        promise.setValue(Result{});
      }
    });
  });
  if (res.fail()) {
    LOG_TOPIC("282be", ERR, Logger::REPLICATION2)
        << "Could not schedule an update after syncing log entries to disk: "
        << res << " Sequence number: " << seq;
  }
}

}  // namespace arangodb::replication2::storage::rocksdb
