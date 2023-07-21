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
/// @author Matthew Von-Maszewski
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// based upon leveldb/util/throttle.cc
// Copyright (c) 2011-2017 Basho Technologies, Inc. All Rights Reserved.
//
// This file is provided to you under the Apache License,
// Version 2.0 (the "License"); you may not use this file
// except in compliance with the License.  You may obtain
// a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBThrottle.h"

#ifndef _WIN32
#include <sys/resource.h>
#endif

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thread>

#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/MetricsFeature.h"

using namespace rocksdb;
using namespace arangodb::metrics;

namespace arangodb {

// rocksdb flushes and compactions start and stop within same thread, no
// overlapping
thread_local std::chrono::steady_clock::time_point flushStart =
    std::chrono::steady_clock::time_point{};

// Setup the object, clearing variables, but do no real work
RocksDBThrottle::RocksDBThrottle(
    uint64_t numSlots, uint64_t frequency, uint64_t scalingFactor,
    uint64_t maxWriteRate, uint64_t slowdownWritesTrigger,
    double fileDescriptorsSlowdownTrigger, double fileDescriptorsStopTrigger,
    double memoryMapsSlowdownTrigger, double memoryMapsStopTrigger,
    uint64_t lowerBoundBps, MetricsFeature& metricsFeature)
    : _internalRocksDB(nullptr),
      _throttleState(ThrottleState::NotStarted),
      _throttleData(std::make_unique<std::vector<ThrottleData_t>>(numSlots)),
      _replaceIdx(2),
      _throttleBps(0),
      _firstThrottle(true),
      _numSlots(numSlots),
      _frequency(frequency),
      _scalingFactor(scalingFactor),
      _maxWriteRate(maxWriteRate == 0 ? std::numeric_limits<uint64_t>::max()
                                      : maxWriteRate),
      _slowdownWritesTrigger(slowdownWritesTrigger),
      _fileDescriptorsSlowdownTrigger(fileDescriptorsSlowdownTrigger),
      _fileDescriptorsStopTrigger(fileDescriptorsStopTrigger),
      _memoryMapsSlowdownTrigger(memoryMapsSlowdownTrigger),
      _memoryMapsStopTrigger(memoryMapsStopTrigger),
      _lowerBoundThrottleBps(lowerBoundBps),
      _fileDescriptorsCurrent(static_cast<Gauge<uint64_t> const*>(
          metricsFeature.get({"arangodb_file_descriptors_current", ""}))),
      _fileDescriptorsLimit(static_cast<Gauge<uint64_t> const*>(
          metricsFeature.get({"arangodb_file_descriptors_limit", ""}))),
      _memoryMapsCurrent(static_cast<Gauge<uint64_t> const*>(
          metricsFeature.get({"arangodb_memory_maps_current", ""}))),
      _memoryMapsLimit(static_cast<Gauge<uint64_t> const*>(
          metricsFeature.get({"arangodb_memory_maps_limit", ""}))) {
  TRI_ASSERT(_scalingFactor != 0);

}

// Shutdown the background thread only if it was ever started
RocksDBThrottle::~RocksDBThrottle() { stopThread(); }

// Shutdown the background thread only if it was ever started
void RocksDBThrottle::stopThread() {
  ThrottleState state = _throttleState.load(std::memory_order_relaxed);

  while (state != ThrottleState::Done) {
    // we cannot shut down while we are currently starting
    if (state == ThrottleState::NotStarted) {
      // NotStarted => Done
      if (_throttleState.compare_exchange_strong(state, ThrottleState::Done)) {
        break;
      }
    } else if (state == ThrottleState::Running) {
      // Started => ShuttingDown
      if (_throttleState.compare_exchange_strong(state,
                                                 ThrottleState::ShuttingDown)) {
        {
          std::lock_guard guard{_threadCondvar.mutex};
          _threadCondvar.cv.notify_one();
        }
        _threadFuture.wait();

        TRI_ASSERT(_throttleState.load() == ThrottleState::ShuttingDown);
        _throttleState.store(ThrottleState::Done);

        {
          std::lock_guard guard{_threadCondvar.mutex};

          _internalRocksDB = nullptr;
        }
        break;
      }
    }

    // wait until startup has finished
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

/// @brief rocksdb does not track flush time in its statistics.  Save start time
/// in a thread specific storage
void RocksDBThrottle::OnFlushBegin(
    rocksdb::DB* db, rocksdb::FlushJobInfo const& flush_job_info) {
  // save start time in thread local storage
  flushStart = std::chrono::steady_clock::now();
}

void RocksDBThrottle::OnFlushCompleted(
    rocksdb::DB* db, rocksdb::FlushJobInfo const& flush_job_info) {
  std::chrono::microseconds flushTime =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - flushStart);
  uint64_t flushSize = flush_job_info.table_properties.data_size +
                       flush_job_info.table_properties.index_size +
                       flush_job_info.table_properties.filter_size;

  setThrottleWriteRate(flushTime, flush_job_info.table_properties.num_entries,
                       flushSize, true);

  // start throttle after first data is posted
  //  (have seen some odd zero and small size flushes early)
  //  (64<<20) is default size for write_buffer_size in column family options,
  //  too hard to read from here
  if ((64 << 19) < flushSize) {
    ThrottleState state = _throttleState.load(std::memory_order_relaxed);
    // call the throttle startup exactly once
    if (state == ThrottleState::NotStarted &&
        _throttleState.compare_exchange_strong(state,
                                               ThrottleState::Starting)) {
      startup(db);
    }
  }
}

void RocksDBThrottle::OnCompactionCompleted(
    rocksdb::DB* db, rocksdb::CompactionJobInfo const& ci) {
  std::chrono::microseconds elapsed(ci.stats.elapsed_micros);
  setThrottleWriteRate(elapsed, ci.stats.num_output_records,
                       ci.stats.total_output_bytes, false);
}

void RocksDBThrottle::startup(rocksdb::DB* db) {
  std::unique_lock guard{_threadCondvar.mutex};

  _internalRocksDB = (rocksdb::DBImpl*)db;

  TRI_ASSERT(_throttleState.load() == ThrottleState::Starting);

  // addresses race condition during fast start/stop.
  // the ThreadLoop will set the _throttleState to Started
  _threadFuture =
      std::async(std::launch::async, &RocksDBThrottle::threadLoop, this);

  while (_throttleState.load() == ThrottleState::Starting) {
    _threadCondvar.cv.wait_for(guard, std::chrono::milliseconds{10});
  }
}

void RocksDBThrottle::setThrottleWriteRate(std::chrono::microseconds micros,
                                           uint64_t keys, uint64_t bytes,
                                           bool isLevel0) {
  TRI_ASSERT(micros.count() >= 0);
  // throw out anything smaller than 32Mbytes ... be better if this
  //  was calculated against write_buffer_size, but that varies by column family
  if ((64 << 19) < bytes) {
    // lock _threadMutex while we update _throttleData
    std::lock_guard mutexLocker{_threadMutex};
    // index 0 for level 0 compactions, index 1 for all others
    unsigned target_idx = (isLevel0 ? 0 : 1);

    auto& throttleData = *_throttleData;

    throttleData[target_idx]._micros += micros;
    throttleData[target_idx]._keys += keys;
    throttleData[target_idx]._bytes += bytes;
    throttleData[target_idx]._compactions += 1;

    // attempt to override throttle changes by rocksdb ... hammer this often
    //  (note that _threadMutex IS HELD)
    setThrottle();
  }

  LOG_TOPIC("7afe9", DEBUG, arangodb::Logger::ENGINES)
      << "SetThrottleWriteRate: micros " << micros.count() << ", keys " << keys
      << ", bytes " << bytes << ", isLevel0 " << isLevel0;
}

void RocksDBThrottle::threadLoop() {
  _replaceIdx = 2;

  // addresses race condition during fast start/stop
  {
    std::lock_guard guard{_threadCondvar.mutex};

    // Starting => Running
    TRI_ASSERT(_throttleState.load() == ThrottleState::Starting);
    _throttleState.store(ThrottleState::Running);
    _threadCondvar.cv.notify_one();
  }

  LOG_TOPIC("a4a57", DEBUG, arangodb::Logger::ENGINES)
      << "ThreadLoop() started";

  while (_throttleState.load(std::memory_order_relaxed) ==
         ThrottleState::Running) {
    // start actual throttle work
    try {
      recalculateThrottle();
    } catch (std::exception const& ex) {
      LOG_TOPIC("b0a2e", WARN, arangodb::Logger::ENGINES)
          << "caught exception in RecalculateThrottle: " << ex.what();
    }

    ++_replaceIdx;
    if (_numSlots == _replaceIdx) {
      _replaceIdx = 2;
    }

    // wait on _threadCondvar
    std::unique_lock guard{_threadCondvar.mutex};

    if (_throttleState.load(std::memory_order_relaxed) ==
        ThrottleState::Running) {
      // test in case of race at shutdown
      _threadCondvar.cv.wait_for(guard, std::chrono::milliseconds{_frequency});
    }
  }

  LOG_TOPIC("eebbe", DEBUG, arangodb::Logger::ENGINES) << "ThreadLoop() ended";
}

// Routine to actually perform the throttle calculation,
//  now is external routing from ThreadLoop() to easy unit test
void RocksDBThrottle::recalculateThrottle() {
  std::chrono::microseconds totalMicros{0};

  auto [compactionBacklog, pendingCompactionBytes] = computeBacklog();

  TRI_ASSERT(_throttleData != nullptr);
  auto& throttleData = *_throttleData;

  uint64_t totalBytes = 0;
  bool noData;
  {
    std::lock_guard mutexLocker{_threadMutex};

    throttleData[_replaceIdx] = throttleData[1];
    throttleData[1] = ThrottleData_t{};

    // this could be faster by keeping running totals and
    //  subtracting [_replaceIdx] before copying [0] into it,
    //  then adding new [_replaceIdx].  But that needs more
    //  time for testing.
    for (uint64_t loop = 2; loop < _numSlots; ++loop) {
      totalMicros += throttleData[loop]._micros;
      totalBytes += throttleData[loop]._bytes;
    }  // for

    // flag to skip throttle changes if zero data available
    noData = (0 == totalBytes && 0 == throttleData[0]._bytes);
  }  // unique_lock

  if (noData) {
    return;
  }

  // reduce bytes by 10% for each excess level_0 files and/or excess write
  // buffers
  uint64_t adjustmentBytes = (totalBytes * compactionBacklog) / 10;

  uint64_t compactionHardLimit =
      _internalRocksDB->GetOptions().hard_pending_compaction_bytes_limit;

  uint64_t adjustmentBytesCor;

  if (compactionHardLimit > 0) {
    // if we are above 25% of the pending compaction bytes stop trigger, take
    // everything into account that is above this threshold, and use it to slow
    // down the writes.
    int64_t threshold = static_cast<int64_t>(compactionHardLimit / 4);
    if (pendingCompactionBytes > threshold) {
      double percentReached =
          static_cast<double>(pendingCompactionBytes - threshold) /
          static_cast<double>(compactionHardLimit - threshold);
      adjustmentBytesCor =
          static_cast<uint64_t>((totalBytes * percentReached) / 2);
    }
  }

  auto fileDescriptorsLimit = _fileDescriptorsLimit->load();
  auto fileDescriptorsCurrent = _fileDescriptorsCurrent->load();
  if (fileDescriptorsLimit > 0) {
    uint64_t threshold = static_cast<int64_t>(fileDescriptorsLimit *
                                              _fileDescriptorsSlowdownTrigger);
    if (fileDescriptorsCurrent > threshold) {
      double percentReached =
          static_cast<double>(fileDescriptorsCurrent - threshold) /
          static_cast<double>(fileDescriptorsLimit - threshold);
      adjustmentBytesCor =
          std::max(adjustmentBytesCor,
                   static_cast<uint64_t>((totalBytes * percentReached) / 2));
    }
  }

  auto memoryMapsLimit = _memoryMapsLimit->load();
  auto memoryMapsCurrent = _memoryMapsCurrent->load();
  if (memoryMapsLimit > 0) {
    uint64_t threshold =
        static_cast<int64_t>(memoryMapsLimit * _memoryMapsSlowdownTrigger);
    if (memoryMapsCurrent > threshold) {
      double percentReached =
          static_cast<double>(memoryMapsCurrent - threshold) /
          static_cast<double>(memoryMapsLimit - threshold);
      adjustmentBytesCor =
          std::max(adjustmentBytesCor,
                   static_cast<uint64_t>((totalBytes * percentReached) / 2));
    }
  }

  adjustmentBytes += adjustmentBytesCor;

  if (adjustmentBytes < totalBytes) {
    totalBytes -= adjustmentBytes;
  } else {
    totalBytes = 1;  // not zero, let smoothing drift number down instead of
                     // taking level-0
  }

  // lock _threadMutex while we update _throttleData
  {
    std::lock_guard mutexLocker{_threadMutex};

    int64_t newThrottle;
    // non-level0 data available?
    if (0 != totalBytes && 0 != totalMicros.count()) {
      // average bytes per second for level 1+ compactions
      //  (adjust bytes upward by 1000000 since dividing by microseconds,
      //   yields integer bytes per second)
      newThrottle = ((totalBytes * 1000000) / totalMicros.count());
    }
    // attempt to most recent level0
    //  (only use most recent level0 until level1+ data becomes available,
    //   useful on restart of heavily loaded server)
    else if (0 != throttleData[0]._bytes &&
             0 != throttleData[0]._micros.count()) {
      newThrottle =
          (throttleData[0]._bytes * 1000000) / throttleData[0]._micros.count();
    } else {
      newThrottle = 1;
    }

    if (0 == newThrottle) {
      newThrottle = 1;  // throttle must have an effect
    }

    // change the throttle slowly
    if (!_firstThrottle) {
      int64_t tempRate = _throttleBps.load(std::memory_order_relaxed);

      if (tempRate < newThrottle) {
        tempRate += (newThrottle - tempRate) / _scalingFactor;
      } else {
        tempRate -= (tempRate - newThrottle) / _scalingFactor;
      }

      if (tempRate < 0) {
        tempRate = 0;
      }

      LOG_TOPIC("46d4a", DEBUG, arangodb::Logger::ENGINES)
          << "RecalculateThrottle(): old " << _throttleBps << ", new "
          << tempRate << ", cap: " << _maxWriteRate
          << ", lower bound: " << _lowerBoundThrottleBps;

      _throttleBps =
          std::max(_lowerBoundThrottleBps,
                   std::min(static_cast<uint64_t>(tempRate), _maxWriteRate));
      TRI_ASSERT(_throttleBps >= _lowerBoundThrottleBps);

      // prepare for next interval
      throttleData[0] = ThrottleData_t{};
    } else if (1 < newThrottle) {
      // never had a valid throttle, and have first hint now
      _throttleBps =
          std::max(_lowerBoundThrottleBps,
                   std::min(static_cast<uint64_t>(newThrottle), _maxWriteRate));

      LOG_TOPIC("e0bbb", DEBUG, arangodb::Logger::ENGINES)
          << "RecalculateThrottle(): first " << _throttleBps;

      _firstThrottle = false;
      TRI_ASSERT(_throttleBps >= _lowerBoundThrottleBps);
    }
  }
}

///
/// @brief Hack a throttle rate into the WriteController object
///
void RocksDBThrottle::setThrottle() {
  // called by routine with _threadMutex held

  // using condition variable's mutex to protect _internalRocksDB race
  std::lock_guard guard{_threadCondvar.mutex};

  // this routine can get called before _internalRocksDB is set
  if (nullptr != _internalRocksDB && _throttleBps > 100) {
    // execute this under RocksDB's DB mutex
    rocksdb::InstrumentedMutexLock db_mutex(_internalRocksDB->mutex());

    // inform write_controller_ of our new rate
    //  (column_family.cc RecalculateWriteStallConditions() makes assumptions
    //   that could force a divide by zero if _throttleBps is less than four
    //   ... using 100 for safety)

    // hard casting away of "const" ...
    auto& writeController =
        const_cast<WriteController&>(_internalRocksDB->write_controller());
    if (writeController.max_delayed_write_rate() < _throttleBps) {
      writeController.set_max_delayed_write_rate(_throttleBps);
    }

    writeController.set_delayed_write_rate(_throttleBps);
  }
}

///
/// @brief Use rocksdb's internal statistics to determine if
///  additional slowing of writes is warranted
/// returns the total number of level0/immutable memtables and the
/// estimated number of bytes to compact, across all column families
std::pair<int64_t, int64_t> RocksDBThrottle::computeBacklog() {
  // want count of level 0 files to estimate if compactions "behind"
  //  and therefore likely to start stalling / stopping
  int64_t immBacklog = 0;
  int64_t immTrigger = 3;
  if (_families.size()) {
    immTrigger =
        _internalRocksDB->GetOptions(_families[0]).max_write_buffer_number / 2;
  }

  int64_t compactionBacklog = 0;
  std::string propertyName;
  std::string retString;
  int64_t pendingCompactionBytes = 0;
  int numLevels = _internalRocksDB->GetOptions().num_levels;
  for (auto const& cf : _families) {
    // loop through column families to obtain family specific counts
    propertyName = rocksdb::DB::Properties::kNumFilesAtLevelPrefix;
    int temp = 0;

    // start at level 0 and then continue digging deeper until we find
    // _some_ file.
    for (int i = 0; i <= numLevels; ++i) {
      propertyName.push_back('0' + i);
      bool ok = _internalRocksDB->GetProperty(cf, propertyName, &retString);
      if (ok) {
        temp = std::stoi(retString);
      } else {
        temp = 0;
      }
      propertyName.pop_back();
      if (temp > 0) {
        // found a file on a level. that is enough.
        break;
      }
    }

    if (static_cast<int>(_slowdownWritesTrigger) <= temp) {
      temp -= (static_cast<int>(_slowdownWritesTrigger) - 1);
    } else {
      temp = 0;
    }

    compactionBacklog += temp;

    propertyName = rocksdb::DB::Properties::kNumImmutableMemTable;
    bool ok = _internalRocksDB->GetProperty(cf, propertyName, &retString);

    if (ok) {
      immBacklog += std::stoi(retString);
    }

    ok = _internalRocksDB->GetProperty(
        cf, rocksdb::DB::Properties::kEstimatePendingCompactionBytes,
        &retString);
    if (ok) {
      pendingCompactionBytes += std::stoll(retString);
    }
  }

  if (immTrigger < immBacklog) {
    compactionBacklog += (immBacklog - immTrigger);
  }

  return {compactionBacklog, pendingCompactionBytes};
}

}  // namespace arangodb
