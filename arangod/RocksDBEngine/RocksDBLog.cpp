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
#include <Basics/ScopeGuard.h>
#include <Basics/Exceptions.h>
#include <Basics/RocksDBUtils.h>
#include <Basics/debugging.h>

#include "RocksDBKey.h"
#include "RocksDBLog.h"
#include "RocksDBValue.h"

using namespace arangodb;
using namespace arangodb::replication2;

RocksDBLog::RocksDBLog(replication2::LogId id, uint64_t objectId,
                       std::shared_ptr<RocksDBLogPersistor> persistor)
    : PersistedLog(id), _objectId(objectId), _persistor(std::move(persistor)) {}

auto RocksDBLog::insert(LogIterator& iter, WriteOptions const& options) -> Result {
  rocksdb::WriteBatch wb;
  auto res = insertWithBatch(iter, wb);
  if (!res.ok()) {
    return res;
  }

  if (auto s = _persistor->_db->Write({}, &wb); !s.ok()) {
    return rocksutils::convertStatus(s);
  }

  if (options.waitForSync) {
    // At this point we have to make sure that every previous log entry is synced
    // as well. Otherwise we might get holes in the log.
    if (auto s = _persistor->_db->SyncWAL(); !s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  return {};
}

struct RocksDBLogIterator : replication2::LogIterator {
  ~RocksDBLogIterator() override = default;

  RocksDBLogIterator(RocksDBLog* log, rocksdb::DB* db,
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

  auto nextImpl() -> std::optional<LogEntry> override {
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

    return std::optional<LogEntry>{std::in_place, RocksDBValue::logTerm(_iter->value()),
                                   RocksDBKey::logIndex(_iter->key()),
                                   RocksDBValue::logPayload(_iter->value())};
  }

  RocksDBKeyBounds const _bounds;
  rocksdb::Slice const _upperBound;
  std::unique_ptr<rocksdb::Iterator> _iter;
  bool _first = true;
};

auto RocksDBLog::read(LogIndex start) -> std::unique_ptr<LogIterator> {
  return std::make_unique<RocksDBLogIterator>(this, _persistor->_db, _persistor->_cf, start);
}

auto RocksDBLog::drop() -> Result {
  TRI_ASSERT(false);
  THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
}

auto RocksDBLog::removeFront(replication2::LogIndex stop) -> Result {
  auto last = RocksDBKey();
  last.constructLogEntry(_objectId, LogIndex{stop.value});

  rocksdb::WriteOptions opts;
  auto s = _persistor->_db->DeleteRange(opts, _persistor->_cf,
                                        getBounds().start(), last.string());
  return rocksutils::convertStatus(s);
}

auto RocksDBLog::removeBack(replication2::LogIndex start) -> Result {
  auto first = RocksDBKey();
  first.constructLogEntry(_objectId, LogIndex{start.value});

  rocksdb::WriteOptions opts;
  auto s = _persistor->_db->DeleteRange(opts, _persistor->_cf, first.string(),
                                        getBounds().end());
  return rocksutils::convertStatus(s);
}

auto RocksDBLog::insertWithBatch(replication2::LogIterator& iter,
                                 rocksdb::WriteBatch& wb) -> Result {
  while (auto entry = iter.next()) {
    auto key = RocksDBKey{};
    key.constructLogEntry(_objectId, entry->logIndex());
    auto value = RocksDBValue::LogEntry(entry->logTerm(), entry->logPayload());
    auto s = wb.Put(_persistor->_cf, key.string(), value.string());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  return Result();
}

auto RocksDBLog::insertAsync(std::unique_ptr<replication2::LogIterator> iter,
                             WriteOptions const& opts) -> futures::Future<Result> {
  RocksDBLogPersistor::WriteOptions wo;
  wo.waitForSync = opts.waitForSync;
  return _persistor->persist(shared_from_this(), std::move(iter), wo);
}

auto RocksDBLog::insertSingleWrites(LogIterator& iter) -> Result {
  while (auto entry = iter.next()) {
    auto key = RocksDBKey{};
    key.constructLogEntry(_objectId, entry->logIndex());
    auto value = RocksDBValue::LogEntry(entry->logTerm(), entry->logPayload());
    auto s = _persistor->_db->Put(rocksdb::WriteOptions{}, _persistor->_cf, key.string(), value.string());
    if (!s.ok()) {
      return rocksutils::convertStatus(s);
    }
  }

  return Result();
}

RocksDBLogPersistor::RocksDBLogPersistor(rocksdb::ColumnFamilyHandle* cf,
                                         rocksdb::DB* db, std::shared_ptr<Executor> executor)
    : _cf(cf), _db(db), _executor(std::move(executor)) {
  // enable wait for sync on lane 0
  _lanes[0]._waitForSync = true;
}

void RocksDBLogPersistor::runPersistorWorker(Lane& lane) noexcept {
  // TODO check this code for exception safety
  // This function is noexcept so in case a exception bubbles up we
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

    auto current = pendingRequests.begin();
    auto start = current;
    try {
      // catch to result is not noexcept
      auto result = basics::catchToResult([&] {
        rocksdb::WriteBatch wb;

        while (current != std::end(pendingRequests)) {
          wb.Clear();

          start = current;
          // We have to make sure that we do not split a single iterator in
          // multiple write batches. Otherwise the in-memory-log could be out
          // of sync.
          while (wb.Count() < 1000 && current != std::end(pendingRequests)) {
            auto* log_ptr = dynamic_cast<RocksDBLog*>(current->log.get());
            TRI_ASSERT(log_ptr != nullptr);
            if (auto res = log_ptr->insertWithBatch(*current->iter, wb); res.fail()) {
              return res;
            }
            ++current;
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

          // resolve all promises in [start, current)
          for (auto it = start; it != current; ++it) {
            it->promise.setValue(TRI_ERROR_NO_ERROR);
          }
        }

        return Result{TRI_ERROR_NO_ERROR};
      });

      // resolve all promises with the result
      if (result.fail()) {
        for (; start != std::end(pendingRequests); ++start) {
          if (!start->promise.isFulfilled()) {
            start->promise.setValue(result);
          }
        }
      }
    } catch (...) {
      auto ex_ptr = std::current_exception();
      for (; start != std::end(pendingRequests); ++start) {
        if (!start->promise.isFulfilled()) {
          start->promise.setException(ex_ptr);
        }
      }
    }
  }
}

auto RocksDBLogPersistor::persist(std::shared_ptr<arangodb::replication2::PersistedLog> log,
                                  std::unique_ptr<arangodb::replication2::LogIterator> iter,
                                  WriteOptions const& options)
    -> futures::Future<Result> {
  auto p = futures::Promise<Result>{};
  auto f = p.getFuture();

  Lane& lane = _lanes[options.waitForSync ? 0 : 1];

  auto lambda =
      fu2::unique_function<void() noexcept>{[self = shared_from_this(), &lane]() noexcept {
        self->runPersistorWorker(lane);
      }};

  {
    std::unique_lock guard(lane._persistorMutex);
    lane._pendingPersistRequests.emplace_back(std::move(log), std::move(iter), std::move(p));
    // TODO this code limits the total amount of threads working in the persistor
    //      maybe we want to limit it for each lane as well?

    // TODO add options for max number of persistor threads for replicated logs
    if (lane._activePersistorThreads == 0 ||
        (lane._pendingPersistRequests.size() > 100 && lane._activePersistorThreads < 2)) {
      // start a new worker thread
      lane._activePersistorThreads += 1;
      guard.unlock();
      _executor->operator()(std::move(lambda));
    }
  }
  return f;
}
