////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// @author Daniel H. Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_THROTTLE_H
#define ARANGO_ROCKSDB_ROCKSDB_THROTTLE_H 1

#include <chrono>
#include <future>

#include "Basics/Common.h"


#include <rocksdb/db.h>
#include <rocksdb/listener.h>

// ugliness starts here ... this will go away if rocksdb adds pluggable write_controller.
using namespace rocksdb;
#define ROCKSDB_PLATFORM_POSIX 1
#include <db/db_impl.h>
#include <db/write_controller.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// If these values change, make sure to reflect the changes in
/// RocksDBPrefixExtractor as well.
////////////////////////////////////////////////////////////////////////////////
class RocksDBEventListener : public rocksdb::EventListener {
public:
  RocksDBEventListener();
  virtual ~RocksDBEventListener();

  void OnFlushBegin(rocksdb::DB* db,
                    const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnFlushCompleted(rocksdb::DB* db,
                        const rocksdb::FlushJobInfo& flush_job_info) override;

  void OnCompactionCompleted(rocksdb::DB* db,
                             const rocksdb::CompactionJobInfo& ci) override;

protected:
  void Startup(rocksdb::DB * db);

  void SetThrottleWriteRate(std::chrono::microseconds Micros,
                            uint64_t Keys, uint64_t Bytes, bool IsLevel0);


  void ThreadLoop();

  // I am unable to figure out static initialization of std::chrono::seconds
  static const unsigned THROTTLE_SECONDS = 60;
  static const unsigned THROTTLE_INTERVALS = 63;

// following is a heristic value, determined by trial and error.
//  its job is slow down the rate of change in the current throttle.
//  do not want sudden changes in one or two intervals to swing
//  the throttle value wildly.  Goal is a nice, even throttle value.
//#define THROTTLE_SCALING 17
  static const unsigned THROTTLE_SCALING = 17;

  struct ThrottleData_t
  {
    std::chrono::microseconds m_Micros;
    uint64_t m_Keys;
    uint64_t m_Bytes;
    //uint64_t m_Backlog;
    uint64_t m_Compactions;
  };

  rocksdb::DBImpl * internalRocksDB_;
  std::atomic<bool> thread_running_;
  std::future<void> thread_future_;

  std::mutex thread_mutex_;
  std::condition_variable thread_condvar_;

  // this array stores compaction statistics used in throttle calculation.
  //  Index 0 of this array accumulates the current minute's compaction data for level 0.
  //  Index 1 accumulates accumulates current minute's compaction
  //  statistics for all other levels.  Remaining intervals contain
  //  most recent interval statistics for last hour.
  ThrottleData_t throttle_data_[THROTTLE_INTERVALS];
  uint64_t throttle_bps_;
  bool first_throttle_;

  std::unique_ptr<WriteControllerToken> delay_token_;

};// class RocksDBEventListener

} // namespace arangodb

#endif
