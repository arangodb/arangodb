////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGO_ROCKSDB_ROCKSDB_THROTTLE_H
#define ARANGO_ROCKSDB_ROCKSDB_THROTTLE_H 1

#include <chrono>
#include <future>

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
using namespace rocksdb;
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

////////////////////////////////////////////////////////////////////////////////
/// If these values change, make sure to reflect the changes in
/// RocksDBPrefixExtractor as well.
////////////////////////////////////////////////////////////////////////////////
class RocksDBThrottle : public rocksdb::EventListener {
 public:
  RocksDBThrottle();
  virtual ~RocksDBThrottle();

  void OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnCompactionCompleted(rocksdb::DB* db, const rocksdb::CompactionJobInfo& ci) override;

  void SetFamilies(std::vector<rocksdb::ColumnFamilyHandle*>& Families) {
    _families = Families;
  }

  static void AdjustThreadPriority(int Adjustment);

  void StopThread();

  uint64_t GetThrottle() const {return _throttleBps;};

 protected:
  void Startup(rocksdb::DB* db);

  void SetThrottleWriteRate(std::chrono::microseconds Micros, uint64_t Keys,
                            uint64_t Bytes, bool IsLevel0);

  void ThreadLoop();

  void SetThrottle();

  int64_t ComputeBacklog();

  void RecalculateThrottle();

  // I am unable to figure out static initialization of std::chrono::seconds,
  //  using old school unsigned.
  static constexpr unsigned THROTTLE_SECONDS = 60;
  static constexpr unsigned THROTTLE_INTERVALS = 63;

  // following is a heristic value, determined by trial and error.
  //  its job is slow down the rate of change in the current throttle.
  //  do not want sudden changes in one or two intervals to swing
  //  the throttle value wildly.  Goal is a nice, even throttle value.
  static constexpr unsigned THROTTLE_SCALING = 17;

  // trigger point where level-0 file is considered "too many pending"
  //  (from original Google leveldb db/dbformat.h)
  static constexpr int64_t kL0_SlowdownWritesTrigger = 8;

  struct ThrottleData_t {
    std::chrono::microseconds _micros;
    uint64_t _keys;
    uint64_t _bytes;
    uint64_t _compactions;
  };

  rocksdb::DBImpl* _internalRocksDB;
  std::once_flag _initFlag;
  std::atomic<bool> _threadRunning;
  std::future<void> _threadFuture;

  Mutex _threadMutex;
  basics::ConditionVariable _threadCondvar;

  // this array stores compaction statistics used in throttle calculation.
  //  Index 0 of this array accumulates the current minute's compaction data for
  //  level 0. Index 1 accumulates accumulates current minute's compaction
  //  statistics for all other levels.  Remaining intervals contain
  //  most recent interval statistics for last hour.
  ThrottleData_t _throttleData[THROTTLE_INTERVALS];
  size_t _replaceIdx;

  std::atomic<uint64_t> _throttleBps;
  bool _firstThrottle;

  std::unique_ptr<WriteControllerToken> _delayToken;
  std::vector<rocksdb::ColumnFamilyHandle*> _families;

};  // class RocksDBThrottle

}  // namespace arangodb

#endif
