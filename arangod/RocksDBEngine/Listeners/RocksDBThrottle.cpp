////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Basics/ConditionLocker.h"
#include "Basics/MutexLocker.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

using namespace rocksdb;

namespace arangodb {

// rocksdb flushes and compactions start and stop within same thread, no
// overlapping
thread_local std::chrono::steady_clock::time_point flushStart =
    std::chrono::steady_clock::time_point{};

// Setup the object, clearing variables, but do no real work
RocksDBThrottle::RocksDBThrottle(uint64_t numSlots, uint64_t frequency,
                                 uint64_t scalingFactor, uint64_t maxWriteRate,
                                 uint64_t slowdownWritesTrigger,
                                 uint64_t lowerBoundBps)
    : _internalRocksDB(nullptr),
      _throttleState(ThrottleState::NotStarted),
      _replaceIdx(2),
      _throttleBps(0),
      _firstThrottle(true),
      _numSlots(numSlots),
      _frequency(frequency),
      _scalingFactor(scalingFactor),
      _maxWriteRate(maxWriteRate == 0 ? std::numeric_limits<uint64_t>::max()
                                      : maxWriteRate),
      _slowdownWritesTrigger(slowdownWritesTrigger),
      _lowerBoundThrottleBps(lowerBoundBps) {
  TRI_ASSERT(_scalingFactor != 0);
  _throttleData = std::make_unique<std::vector<ThrottleData_t>>();
  _throttleData->resize(numSlots);
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
          CONDITION_LOCKER(guard, _threadCondvar);
          _threadCondvar.signal();
        }
        _threadFuture.wait();

        TRI_ASSERT(_throttleState.load() == ThrottleState::ShuttingDown);
        _throttleState.store(ThrottleState::Done);

        {
          CONDITION_LOCKER(guard, _threadCondvar);

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
    rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {
  // save start time in thread local storage
  flushStart = std::chrono::steady_clock::now();
}

void RocksDBThrottle::OnFlushCompleted(
    rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {
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
    rocksdb::DB* db, const rocksdb::CompactionJobInfo& ci) {
  std::chrono::microseconds elapsed(ci.stats.elapsed_micros);
  setThrottleWriteRate(elapsed, ci.stats.num_output_records,
                       ci.stats.total_output_bytes, false);
}

void RocksDBThrottle::startup(rocksdb::DB* db) {
  CONDITION_LOCKER(guard, _threadCondvar);

  _internalRocksDB = (rocksdb::DBImpl*)db;

  TRI_ASSERT(_throttleState.load() == ThrottleState::Starting);

  // addresses race condition during fast start/stop.
  // the ThreadLoop will set the _throttleState to Started
  _threadFuture =
      std::async(std::launch::async, &RocksDBThrottle::threadLoop, this);

  while (_throttleState.load() == ThrottleState::Starting) {
    _threadCondvar.wait(10000);
  }
}

void RocksDBThrottle::setThrottleWriteRate(std::chrono::microseconds Micros,
                                           uint64_t Keys, uint64_t Bytes,
                                           bool IsLevel0) {
  TRI_ASSERT(Micros.count() >= 0);
  // throw out anything smaller than 32Mbytes ... be better if this
  //  was calculated against write_buffer_size, but that varies by column family
  if ((64 << 19) < Bytes) {
    // lock _threadMutex while we update _throttleData
    MUTEX_LOCKER(mutexLocker, _threadMutex);
    // index 0 for level 0 compactions, index 1 for all others
    unsigned target_idx = (IsLevel0 ? 0 : 1);

    auto& throttleData = *_throttleData;

    throttleData[target_idx]._micros += Micros;
    throttleData[target_idx]._keys += Keys;
    throttleData[target_idx]._bytes += Bytes;
    throttleData[target_idx]._compactions += 1;

    // attempt to override throttle changes by rocksdb ... hammer this often
    //  (note that _threadMutex IS HELD)
    setThrottle();
  }

  LOG_TOPIC("7afe9", DEBUG, arangodb::Logger::ENGINES)
      << "SetThrottleWriteRate: Micros " << Micros.count() << ", Keys " << Keys
      << ", Bytes " << Bytes << ", IsLevel0 " << IsLevel0;
}

void RocksDBThrottle::threadLoop() {
  _replaceIdx = 2;

  // addresses race condition during fast start/stop
  {
    CONDITION_LOCKER(guard, _threadCondvar);

    // Starting => Running
    TRI_ASSERT(_throttleState.load() == ThrottleState::Starting);
    _throttleState.store(ThrottleState::Running);
    _threadCondvar.signal();
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
    CONDITION_LOCKER(guard, _threadCondvar);

    if (_throttleState.load(std::memory_order_relaxed) ==
        ThrottleState::Running) {
      // test in case of race at shutdown
      _threadCondvar.wait(std::chrono::microseconds(_frequency * 1000));
    }
  }

  LOG_TOPIC("eebbe", DEBUG, arangodb::Logger::ENGINES) << "ThreadLoop() ended";
}

// Routine to actually perform the throttle calculation,
//  now is external routing from ThreadLoop() to easy unit test
void RocksDBThrottle::recalculateThrottle() {
  std::chrono::microseconds totalMicros{0};
  uint64_t totalBytes = 0;
  bool noData;

  auto [compactionBacklog, pendingCompactionBytes] = computeBacklog();
  TRI_ASSERT(_throttleData != nullptr);
  auto& throttleData = *_throttleData;

  {
    MUTEX_LOCKER(mutexLocker, _threadMutex);

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

  // reduce bytes by 10% for each excess level_0 files and/or excess write
  // buffers
  uint64_t adjustmentBytes = (totalBytes * compactionBacklog) / 10;

  uint64_t compactionHardLimit =
      _internalRocksDB->GetOptions().hard_pending_compaction_bytes_limit;
  if (compactionHardLimit > 0) {
    // if we are above 25% of the pending compaction bytes stop trigger, take
    // everything into account that is above this threshold, and use it to slow
    // down the writes.
    int64_t threshold = static_cast<int64_t>(compactionHardLimit / 4);
    if (pendingCompactionBytes > threshold) {
      double percentReached =
          static_cast<double>(pendingCompactionBytes - threshold) /
          static_cast<double>(compactionHardLimit - threshold);
      adjustmentBytes +=
          static_cast<uint64_t>((totalBytes * percentReached) / 2);
    }
  }

  if (adjustmentBytes < totalBytes) {
    totalBytes -= adjustmentBytes;
  } else {
    totalBytes = 1;  // not zero, let smoothing drift number down instead of
                     // taking level-0
  }

  // lock _threadMutex while we update _throttleData
  if (!noData) {
    MUTEX_LOCKER(mutexLocker, _threadMutex);

    int64_t newThrottle;
    // non-level0 data available?
    if (0 != totalBytes && 0 != totalMicros.count()) {
      // average bytes per second for level 1+ compactions
      //  (adjust bytes upward by 1000000 since dividing by microseconds,
      //   yields integer bytes per second)
      newThrottle = ((totalBytes * 1000000) / totalMicros.count());
    }  // if

    // attempt to most recent level0
    //  (only use most recent level0 until level1+ data becomes available,
    //   useful on restart of heavily loaded server)
    else if (0 != throttleData[0]._bytes &&
             0 != throttleData[0]._micros.count()) {
      newThrottle =
          (throttleData[0]._bytes * 1000000) / throttleData[0]._micros.count();
    }  // else if
    else {
      newThrottle = 1;
    }  // else

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

      // +2 can make this go negative
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
    }  // else if
  }
}

///
/// @brief Hack a throttle rate into the WriteController object
///
void RocksDBThrottle::setThrottle() {
  // called by routine with _threadMutex held

  // using condition variable's mutex to protect _internalRocksDB race
  {
    CONDITION_LOCKER(guard, _threadCondvar);

    // this routine can get called before _internalRocksDB is set
    if (nullptr != _internalRocksDB) {
      // execute this under RocksDB's DB mutex
      rocksdb::InstrumentedMutexLock db_mutex(_internalRocksDB->mutex());

      // inform write_controller_ of our new rate
      //  (column_family.cc RecalculateWriteStallConditions() makes assumptions
      //   that could force a divide by zero if _throttleBps is less than four
      //   ... using 100 for safety)
      if (100 < _throttleBps) {
        // hard casting away of "const" ...
        auto& writeController =
            const_cast<WriteController&>(_internalRocksDB->write_controller());
        if (writeController.max_delayed_write_rate() < _throttleBps) {
          writeController.set_max_delayed_write_rate(_throttleBps);
        }

        writeController.set_delayed_write_rate(_throttleBps);
      }
    }
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
