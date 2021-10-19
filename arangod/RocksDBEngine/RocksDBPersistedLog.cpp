////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
#include <Replication2/ReplicatedLog/PersistedLog.h>
#include <memory>

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "RocksDBKey.h"
#include "RocksDBPersistedLog.h"
#include "RocksDBValue.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

RocksDBPersistedLog::RocksDBPersistedLog(replication2::LogId id, uint64_t objectId,
                                         std::shared_ptr<RocksDBLogPersistor> persistor)
    : PersistedLog(id), _objectId(objectId), _persistor(std::move(persistor)) {}

auto RocksDBPersistedLog::insert(PersistedLogIterator& iter, WriteOptions const& options)
    -> Result {
  rocksdb::WriteBatch wb;
  auto res = prepareWriteBatch(iter, wb);
  if (!res.ok()) {
    return res;
  }

  if (auto s = _persistor->_db->Write({}, &wb); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  if (options.waitForSync) {
    // At this point we have to make sure that every previous log entry is
    // synced as well. Otherwise we might get holes in the log.
    if (auto s = _persistor->_db->SyncWAL(); !s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  return {};
}

struct RocksDBLogIterator : PersistedLogIterator {
  ~RocksDBLogIterator() override = default;

  RocksDBLogIterator(RocksDBPersistedLog* log, rocksdb::DB* db,
                     rocksdb::ColumnFamilyHandle* cf, LogIndex start)
      : _bounds(log->getBounds()), _upperBound(_bounds.end()) {
    rocksdb::ReadOptions opts;
    opts.prefix_same_as_start = true;
    opts.iterate_upper_bound = &_upperBound;
    _iter.reset(db->NewIterator(opts, cf));

    auto first = RocksDBKey{};
    first.constructLogEntry(log->objectId(), start);
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

    return std::optional<PersistingLogEntry>{std::in_place,
                                             RocksDBKey::logIndex(_iter->key()),
                                             RocksDBValue::data(_iter->value())};
  }

  RocksDBKeyBounds const _bounds;
  rocksdb::Slice const _upperBound;
  std::unique_ptr<rocksdb::Iterator> _iter;
  bool _first = true;
};

auto RocksDBPersistedLog::read(LogIndex start) -> std::unique_ptr<PersistedLogIterator> {
  return std::make_unique<RocksDBLogIterator>(this, _persistor->_db, _persistor->_cf, start);
}

auto RocksDBPersistedLog::drop() -> Result {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto RocksDBPersistedLog::removeFront(replication2::LogIndex stop) -> Result {
  auto last = RocksDBKey();
  last.constructLogEntry(_objectId, stop);

  rocksdb::WriteOptions opts;
  auto s = _persistor->_db->DeleteRange(opts, _persistor->_cf,
                                        getBounds().start(), last.string());
  return rocksutils::convertStatus(s);
}

auto RocksDBPersistedLog::removeBack(replication2::LogIndex start) -> Result {
  auto first = RocksDBKey();
  first.constructLogEntry(_objectId, start);

  rocksdb::WriteOptions opts;
  auto s = _persistor->_db->DeleteRange(opts, _persistor->_cf, first.string(),
                                        getBounds().end());
  return rocksutils::convertStatus(s);
}

auto RocksDBPersistedLog::prepareWriteBatch(PersistedLogIterator& iter,
                                            rocksdb::WriteBatch& wb) -> Result {
  auto key = RocksDBKey{};
  while (auto entry = iter.next()) {
    key.constructLogEntry(_objectId, entry->logIndex());
    auto value = RocksDBValue::LogEntry(*entry);
    auto s = wb.Put(_persistor->_cf, key.string(), value.string());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  return Result();
}

auto RocksDBPersistedLog::insertAsync(std::unique_ptr<PersistedLogIterator> iter,
                                      WriteOptions const& opts) -> futures::Future<Result> {
  RocksDBLogPersistor::WriteOptions wo;
  wo.waitForSync = opts.waitForSync;
  return _persistor->persist(shared_from_this(), std::move(iter), wo);
}

RocksDBLogPersistor::RocksDBLogPersistor(rocksdb::ColumnFamilyHandle* cf,
                                         rocksdb::DB* db, std::shared_ptr<Executor> executor,
                                         std::shared_ptr<replication2::ReplicatedLogOptions const> options)
    : _cf(cf), _db(db), _executor(std::move(executor)), _options(std::move(options)) {}

void RocksDBLogPersistor::runPersistorWorker(Lane& lane) noexcept {
  // This function is noexcept so in case an exception bubbles up we
  // rather crash instead of losing one thread.
  while (true) {
    std::vector<PersistRequest> pendingRequests;
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
    }

    auto nextReqToWrite = pendingRequests.begin();
    auto nextReqToResolve = nextReqToWrite;

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
        while (wb.GetDataSize() < _options->_maxRocksDBWriteBatchSize &&
               nextReqToWrite != std::end(pendingRequests)) {
          auto* log_ptr = dynamic_cast<RocksDBPersistedLog*>(nextReqToWrite->log.get());
          TRI_ASSERT(log_ptr != nullptr);
          if (auto res = log_ptr->prepareWriteBatch(*nextReqToWrite->iter, wb);
              res.fail()) {
            return res;
          }
          TRI_ASSERT(nextReqToWrite->iter->next() == std::nullopt);
          ++nextReqToWrite;
        }
        {
          if (auto s = _db->Write({}, &wb); !s.ok()) {
            return rocksutils::convertStatus(s);
          }

          if (lane._waitForSync) {
            if (auto s = _db->SyncWAL(); !s.ok()) {
              // At this point we have to make sure that every previous log entry is synced
              // as well. Otherwise we might get holes in the log.
              return rocksutils::convertStatus(s);
            }
          }
        }

        // resolve all promises in [nextReqToResolve, nextReqToWrite)
        for (; nextReqToResolve != nextReqToWrite; ++nextReqToResolve) {
          nextReqToResolve->promise.setValue(TRI_ERROR_NO_ERROR);
        }
      }

      return Result{TRI_ERROR_NO_ERROR};
    });

    // resolve all promises with the result
    if (result.fail()) {
      for (; nextReqToResolve != std::end(pendingRequests); ++nextReqToResolve) {
        // If a promise is fulfilled before (with a value), nextReqToResolve
        // should always be increased as well; meaning we only exactly iterate
        // over the unfulfilled promises here.
        TRI_ASSERT(!nextReqToResolve->promise.isFulfilled());
        nextReqToResolve->promise.setValue(result);
      }
    }
  }
}

auto RocksDBLogPersistor::persist(std::shared_ptr<PersistedLog> log,
                                  std::unique_ptr<PersistedLogIterator> iter,
                                  WriteOptions const& options) -> futures::Future<Result> {
  auto p = futures::Promise<Result>{};
  auto f = p.getFuture();

  Lane& lane = _lanes[options.waitForSync ? 0 : 1];
  TRI_ASSERT(lane._waitForSync == options.waitForSync);

  {
    std::unique_lock guard(lane._persistorMutex);
    lane._pendingPersistRequests.emplace_back(std::move(log), std::move(iter),
                                              std::move(p));
    bool const wantNewThread = lane._activePersistorThreads == 0 ||
                               (lane._pendingPersistRequests.size() > 100 &&
                                lane._activePersistorThreads < 2);

    if (!wantNewThread) {
      return f;
    } else {
      lane._activePersistorThreads += 1;
    }
  }

  // We committed ourselves to start a thread
  size_t num_retries = 0;
  while (true) {
    auto lambda =
        fu2::unique_function<void() noexcept>{[self = shared_from_this(), &lane]() noexcept {
          self->runPersistorWorker(lane);
        }};

    try {
      _executor->operator()(std::move(lambda));  // may throw a QUEUE_FULL exception
      break;
    } catch (std::exception const& e) {
      LOG_TOPIC("213cb", WARN, Logger::REPLICATION2)
          << "Could not post persistence request onto the scheduler: " << e.what()
          << " Retries: " << num_retries;
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

  return f;
}
