////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBRateLimiterThread.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogMacros.h"
#include "RocksDBEngine/RocksDBEngine.h"

#include <rocksdb/db.h>
#include <rocksdb/statistics.h>
#include <rocksdb/utilities/transaction_db.h>

// NOT public rocksdb headers
// ugliness starts here ... this will go away if rocksdb adds pluggable
// write_controller.
#define ROCKSDB_PLATFORM_POSIX 1
#include <db/db_impl/db_impl.h>
#include <db/write_controller.h>

#include <limits>

using namespace arangodb;

namespace {
// rocksdb does not track flush time in its statistics. save start time
// in a thread-local storage.
// rocksdb flushes and compactions start and stop within same thread, so
// no overlapping can happen.
thread_local std::chrono::steady_clock::time_point flushStart =
    std::chrono::steady_clock::time_point{};
}  // namespace

RocksDBRateLimiterThread::RocksDBRateLimiterThread(
    RocksDBEngine& engine, uint64_t numSlots, uint64_t frequency,
    uint64_t scalingFactor, uint64_t minWriteRate, uint64_t maxWriteRate,
    uint64_t slowdownWritesTrigger)
    : Thread(engine.server(), "RocksDBRateLimiter"),
      _engine(engine),
      _db(nullptr),
      _numSlots(numSlots),
      _frequency(frequency),
      _scalingFactor(scalingFactor),
      _slowdownWritesTrigger(slowdownWritesTrigger),
      _minWriteRate(minWriteRate),
      _maxWriteRate(maxWriteRate),
      _currentRate(0),
      _currentHistoryIndex(0) {
  if (_maxWriteRate == 0) {
    _maxWriteRate = std::numeric_limits<decltype(_maxWriteRate)>::max();
  } else {
    _maxWriteRate = std::max(_maxWriteRate, _minWriteRate);
  }
  TRI_ASSERT(_minWriteRate <= _maxWriteRate);

  _history.resize(_numSlots);
}

RocksDBRateLimiterThread::~RocksDBRateLimiterThread() { shutdown(); }

void RocksDBRateLimiterThread::OnFlushBegin(
    rocksdb::DB* db, rocksdb::FlushJobInfo const& flush_job_info) {
  // save start time in thread-local storage
  ::flushStart = std::chrono::steady_clock::now();
}

void RocksDBRateLimiterThread::OnFlushCompleted(
    rocksdb::DB* db, rocksdb::FlushJobInfo const& flush_job_info) {
  // pick up flush start time from thread-local storage and calculate
  // duration
  std::chrono::microseconds flushTime =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - ::flushStart);
  uint64_t flushSize = flush_job_info.table_properties.data_size +
                       flush_job_info.table_properties.index_size +
                       flush_job_info.table_properties.filter_size;

  for (auto const& blob : flush_job_info.blob_file_addition_infos) {
    flushSize += blob.total_blob_bytes;
  }

  // update values in current history slot
  std::lock_guard<std::mutex> guard{_mutex};
  TRI_ASSERT(_currentHistoryIndex < _history.size());
  _history[_currentHistoryIndex].first += flushSize;
  _history[_currentHistoryIndex].second += flushTime;

  LOG_DEVEL << "FLUSH COMPLETED. " << flushSize
            << ", TIME: " << flushTime.count()
            << ", INDEX: " << _currentHistoryIndex;
}

void RocksDBRateLimiterThread::OnCompactionCompleted(
    rocksdb::DB* db, rocksdb::CompactionJobInfo const& ci) {}

void RocksDBRateLimiterThread::beginShutdown() {
  Thread::beginShutdown();

  std::lock_guard guard{_mutex};
  _cv.notify_all();
}

void RocksDBRateLimiterThread::run() {
  while (!isStopping()) {
    try {
      std::unique_lock guard{_mutex};

      // don't throttle while we are still in recovery or have not been
      // initialized properly
      if (!_engine.inRecovery() && !_families.empty()) {
        TRI_ASSERT(_db != nullptr);

        // sum up all recorded values from history
        uint64_t totalSize = 0;
        std::chrono::microseconds totalTime{};
        size_t filledSlots = 0;

        for (auto const& it : _history) {
          if (it.first > 0) {
            totalSize += it.first;
            totalTime += it.second;
            ++filledSlots;
          }
        }

        // if we have too few data points (e.g. less than 3), it is not good
        // to use an average. only do the averaging and adjust the write rate
        // in case enough things happened and we have enough data to do the
        // averages.
        if (filledSlots >= 3) {
          auto tc = totalTime.count();
          TRI_ASSERT(tc > 0);

          uint64_t targetRate = (totalSize * 1'000'000) / tc;

          uint64_t pendingCompactionBytes = computePendingCompactionBytes();

          auto internalRocksDB = dynamic_cast<rocksdb::DBImpl*>(_db);

          double percentReached = 0.0;
          if (uint64_t compactionHardLimit =
                  internalRocksDB->GetOptions()
                      .hard_pending_compaction_bytes_limit;
              compactionHardLimit > 0) {
            // if we are above 25% of the pending compaction bytes stop trigger,
            // take everything into account that is above this threshold, and
            // use it to slow down the writes.
            uint64_t threshold = compactionHardLimit / 4;
            if (pendingCompactionBytes > threshold) {
              // we are above the threshold, so penalize writes with compaction
              // debt
              percentReached =
                  static_cast<double>(pendingCompactionBytes - threshold) /
                  static_cast<double>(compactionHardLimit - threshold);
              targetRate -= (1.0 - percentReached) * targetRate;
            }
          }

          uint64_t oldRate = _currentRate.load(std::memory_order_relaxed);
          uint64_t newRate = std::invoke([&]() {
            if (oldRate == 0) {
              return targetRate;
            }
            if (targetRate > oldRate) {
              return oldRate + (targetRate - oldRate) / _scalingFactor;
            } else {
              return oldRate - (oldRate - targetRate) / _scalingFactor;
            }
          });

          newRate = std::clamp(newRate, _minWriteRate, _maxWriteRate);

          LOG_DEVEL << "TOTAL BYTES: " << totalSize
                    << ", TARGET: " << targetRate << ", OLD: " << oldRate
                    << ", NEW: " << newRate << ", DIFF: " << (newRate - oldRate)
                    << ", PENDING COMPACTION: " << pendingCompactionBytes
                    << " (" << (percentReached * 100.0) << "%)";

          // update global rate
          _currentRate.store(newRate, std::memory_order_relaxed);

          // adjust value in RocksDB
          // execute this under RocksDB's DB mutex
          rocksdb::InstrumentedMutexLock db_mutex(internalRocksDB->mutex());
          auto& writeController = const_cast<rocksdb::WriteController&>(
              internalRocksDB->write_controller());
          if (writeController.max_delayed_write_rate() < newRate) {
            writeController.set_max_delayed_write_rate(newRate);
          }
          writeController.set_delayed_write_rate(newRate);
        }

        // bump current history index and check for index overflow
        if (++_currentHistoryIndex >= _history.size()) {
          _currentHistoryIndex = 0;
        }
        // and clear current entry for all things to come
        _history[_currentHistoryIndex] = {};
      }

      // wait until we are woken up
      _cv.wait_for(guard, std::chrono::milliseconds(_frequency));
    } catch (std::exception const& ex) {
      LOG_TOPIC("75584", WARN, Logger::ENGINES)
          << "caught exception in rocksdb rate limiter thread: " << ex.what();
    }
  }
}

uint64_t RocksDBRateLimiterThread::currentRate() const noexcept {
  return _currentRate.load(std::memory_order_relaxed);
}

void RocksDBRateLimiterThread::setFamilies(
    std::vector<rocksdb::ColumnFamilyHandle*> const& families) {
  TRI_ASSERT(!families.empty());

  std::unique_lock guard{_mutex};
  TRI_ASSERT(_families.empty());
  _families = families;

  _db = _engine.db()->GetRootDB();
  TRI_ASSERT(_db != nullptr);
}

uint64_t RocksDBRateLimiterThread::computePendingCompactionBytes() const {
  TRI_ASSERT(_db != nullptr);
  int64_t pendingCompactionBytes = 0;

  std::string result;
  for (auto const& cf : _families) {
    if (_db->GetProperty(
            cf, rocksdb::DB::Properties::kEstimatePendingCompactionBytes,
            &result)) {
      pendingCompactionBytes += std::stoll(result);
    }
  }
  return pendingCompactionBytes;
}
