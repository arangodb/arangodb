////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <chrono>
#include <future>
#include <memory>
#include <vector>

#include "Basics/Common.h"
#include "Basics/ConditionVariable.h"
#include "Basics/Mutex.h"

// public rocksdb headers
#include <rocksdb/db.h>
#include <rocksdb/listener.h>

// NOT public rocksdb headers
// ugliness starts here ... this will go away if rocksdb adds pluggable
// write_controller.
//  need either ROCKSDB_PLATFORM_POSIX or OS_WIN set before the <db/...>
//  includes
#ifndef _WIN32
#define ROCKSDB_PLATFORM_POSIX 1
#else
#define OS_WIN 1
#include <sys/types.h>
#define _SSIZE_T_DEFINED 1
#endif
#include <db/db_impl/db_impl.h>
#include <db/write_controller.h>

namespace arangodb {

class RocksDBThrottle : public rocksdb::EventListener {
 public:
  RocksDBThrottle(uint64_t numSlots, uint64_t frequency, uint64_t scalingFactor, 
                  uint64_t maxWriteRate, uint64_t slowdownWritesTrigger);
  virtual ~RocksDBThrottle();

  void OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnCompactionCompleted(rocksdb::DB* db, const rocksdb::CompactionJobInfo& ci) override;

  void SetFamilies(std::vector<rocksdb::ColumnFamilyHandle*>& Families) {
    _families = Families;
  }

  void stopThread();

  uint64_t GetThrottle() const { return _throttleBps; }

 protected:
  void startup(rocksdb::DB* db);

  void SetThrottleWriteRate(std::chrono::microseconds Micros, uint64_t Keys,
                            uint64_t Bytes, bool IsLevel0);

  void ThreadLoop();

  void SetThrottle();

  int64_t ComputeBacklog();

  void RecalculateThrottle();

  struct ThrottleData_t {
    std::chrono::microseconds _micros{};
    uint64_t _keys = 0;
    uint64_t _bytes = 0;
    uint64_t _compactions = 0;

    ThrottleData_t() noexcept = default;
  };

  rocksdb::DBImpl* _internalRocksDB;
  std::future<void> _threadFuture;

  /// state of the throttle. the state will always be advanced from a 
  /// lower to a higher number (e.g. from NotStarted to Starting, 
  /// from Starting to Running etc.) but never vice versa. It is possible
  /// jump from NotStarted to Done directly, but otherwise the sequence
  /// is NotStarted => Starting => Running => ShuttingDown => Done
  enum class ThrottleState {
    NotStarted    = 1, // not started, this is the state at the beginning
    Starting      = 2, // while background thread is started
    Running       = 3, // throttle is operating normally
    ShuttingDown  = 4, // throttle is in shutdown
    Done          = 5, // throttle is shutdown
  };
  std::atomic<ThrottleState> _throttleState;

  Mutex _threadMutex;
  basics::ConditionVariable _threadCondvar;

  // this array stores compaction statistics used in throttle calculation.
  //  Index 0 of this array accumulates the current interval's compaction data for
  //  level 0. Index 1 accumulates accumulates current intervals's compaction
  //  statistics for all other levels.  Remaining intervals contain
  //  most recent interval statistics for the total time period.
  std::unique_ptr<std::vector<ThrottleData_t>> _throttleData;
  size_t _replaceIdx;

  std::atomic<uint64_t> _throttleBps;
  bool _firstThrottle;

  std::unique_ptr<rocksdb::WriteControllerToken> _delayToken;
  std::vector<rocksdb::ColumnFamilyHandle*> _families;

 private:
  uint64_t const _numSlots;
  // frequency in milliseconds
  uint64_t const _frequency; 
  uint64_t const _scalingFactor;
  uint64_t const _maxWriteRate;
  uint64_t const _slowdownWritesTrigger;
}; 

}  // namespace arangodb
