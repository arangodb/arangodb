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

#include <sys/syscall.h>
#include <sys/resource.h>

namespace arangodb {

// rocksdb flushes and compactions start and stop within same thread, no overlapping
thread_local std::chrono::steady_clock::time_point gFlushStart;


////////////////////////////////////////////////////////////////////////////////
/// AdjustThreadPriority() below uses the Linux setpriority() function to dynamically
///  lower and raise a given thread's scheduling priority.  The Linux default is
///  to only allow a thread to lower its priority, not to raise it.  Even if the
///  raise would be to a previous priority.
///
/// To get full performance benefit of this code, the user's needs three settings:
///
///  1. /etc/pam.d/login must contain the line "auth	   require    pam_cap.so"
///  2. /etc/security/capability.conf must contain "cap_sys_nice      arangodb"
///  3. root must execute this command "setcap cap_sys_nice+ie arangod" on
///      the arangodb binary executable
///
////////////////////////////////////////////////////////////////////////////////

// code will dynamically change a thread's priority based upon the compaction's level:
//  base +1 : flush mem buffer to level 0
//  base +2 : level 0 compaction to level 1
//  base +3 : all other compactions
struct sPriorityInfo {
  bool _baseSet;
  int _basePriority;
  int _currentPriority;
};

thread_local sPriorityInfo gThreadPriority={false, 0, 0};

///
/// @brief Object that RocksDBThrottle gives to a compaction thread
///
class RocksDBCompactionListener : public rocksdb::CompactionEventListener {

  ///
  /// @brief This is called for every key in a compaction.  Our code only uses "level"
  ///  to help manipulate thread priority
  void OnCompaction(int level, const Slice& key,
                    CompactionListenerValueType value_type,
                    const Slice& existing_value,
                    const SequenceNumber& sn, bool is_new) override
  {RocksDBThrottle::AdjustThreadPriority( (0==level) ? 2 : 3);};

};// RocksDBCompactionListener

RocksDBCompactionListener gCompactionListener;


//
// Setup the object, clearing variables, but do no real work
//
RocksDBThrottle::RocksDBThrottle()
  : internalRocksDB_(nullptr), thread_running_(false), throttle_bps_(0), first_throttle_(true)
{
  memset(&throttle_data_, 0, sizeof(throttle_data_));
}


//
// Shutdown the background thread only if it was ever started
//
RocksDBThrottle::~RocksDBThrottle() {

  if (thread_running_.load()) {
    thread_running_.store(false);

    thread_condvar_.notify_one();
    thread_future_.wait();
  } // if
}


///
/// @brief CompactionEventListener::OnCompaction() will get called for every key in a
///  compaction. We only need the "level" parameter to see if thread priority should change.
///
//#ifdef OS_LINUX
#if 1
CompactionEventListener * RocksDBThrottle::GetCompactionEventListener() {return &gCompactionListener;};
#else
CompactionEventListener * RocksDBThrottle::GetCompactionEventListener() {return nullptr;};
#endif

///
/// @brief rocksdb does not track flush time in its statistics.  Save start time in
///  a thread specific storage
///
void RocksDBThrottle::OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {

  // save start time in thread local storage
  gFlushStart = std::chrono::steady_clock::now();
  AdjustThreadPriority(1);

  return;

} // RocksDBThrottle::OnFlushBegin


void RocksDBThrottle::OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {
  std::chrono::microseconds flush_time;
  uint64_t flush_size;

  flush_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - gFlushStart);
  flush_size = flush_job_info.table_properties.data_size + flush_job_info.table_properties.index_size
    + flush_job_info.table_properties.filter_size;

  SetThrottleWriteRate(flush_time, flush_job_info.table_properties.num_entries, flush_size, true);

  // start throttle after first data is posted
  //  (have seen some odd zero and small size flushes early)
  if (1024<flush_size) {
    std::call_once(init_flag_, &RocksDBThrottle::Startup, this, db);
  } // if

} // RocksDBThrottle::OnFlushCompleted


void RocksDBThrottle::OnCompactionCompleted(rocksdb::DB* db,
                                                 const rocksdb::CompactionJobInfo& ci) {

  std::chrono::microseconds elapsed(ci.stats.elapsed_micros);
  SetThrottleWriteRate(elapsed, ci.stats.num_output_records, ci.stats.total_output_bytes, false);

} // RocksDBThrottle::OnCompactionCompleted


void RocksDBThrottle::Startup(rocksdb::DB* db) {
  internalRocksDB_ = (rocksdb::DBImpl *)db;

  // addresses race condition during fast start/stop
  {
    std::unique_lock<std::mutex> ul(thread_mutex_);

    thread_future_ = std::async(std::launch::async, &RocksDBThrottle::ThreadLoop, this);

    while(!thread_running_.load())
      thread_condvar_.wait(ul);
  }   // mutex

} // RocksDBThrottle::Startup


void RocksDBThrottle::SetThrottleWriteRate(std::chrono::microseconds Micros,
                                                uint64_t Keys, uint64_t Bytes, bool IsLevel0) {
  // lock thread_mutex_ while we update throttle_data_
  std::unique_lock<std::mutex> ul(thread_mutex_);
  unsigned target_idx;

  // index 0 for level 0 compactions, index 1 for all others
  target_idx = (IsLevel0 ? 0 : 1);

  throttle_data_[target_idx]._micros+=Micros;
  throttle_data_[target_idx]._keys+=Keys;
  throttle_data_[target_idx]._bytes+=Bytes;
  throttle_data_[target_idx]._compactions+=1;

  // attempt to override throttle changes by rocksdb ... hammer this often
  //  (note that thread_mutex_ IS HELD)
  SetThrottle();

  return;
};


void RocksDBThrottle::ThreadLoop() {

  replace_idx_=2;

  // addresses race condition during fast start/stop
  {
    std::unique_lock<std::mutex> ul(thread_mutex_);
    thread_running_.store(true);
    thread_condvar_.notify_one();
  }   // mutex

  while(thread_running_.load()) {
    //
    // start actual throttle work
    //
    RecalculateThrottle();

    ++replace_idx_;
    if (THROTTLE_INTERVALS==replace_idx_)
      replace_idx_=2;

    {
      // lock thread_mutex_ while we update throttle_data_ and
      // wait on thread_condvar_
      std::unique_lock<std::mutex> ul(thread_mutex_);

      // sleep 1 minute
      std::chrono::seconds wait_seconds(THROTTLE_SECONDS);

      if (thread_running_.load()) { // test in case of race at shutdown
        thread_condvar_.wait_for(ul, wait_seconds);
      } //if
    } // unlock thread_mutex_

  } // while

} // RocksDBThrottle::ThreadLoop


//
// Routine to actually perform the throttle calculation,
//  now is external routing from ThreadLoop() to easy unit test
void RocksDBThrottle::RecalculateThrottle() {
  unsigned loop;
  std::chrono::microseconds tot_micros;
  uint64_t tot_bytes, tot_keys, tot_compact, adjustment_bytes;
  int64_t new_throttle, compaction_backlog, temp_rate;
  bool no_data;

  tot_micros*=0;
  tot_keys=0;
  tot_bytes=0;
  tot_compact=0;
  temp_rate=0;

  compaction_backlog = ComputeBacklog();

  {
    std::unique_lock<std::mutex> ul(thread_mutex_);

    throttle_data_[replace_idx_]=throttle_data_[1];
    //throttle_data_[replace_idx_].m_Backlog=0;
    memset(&throttle_data_[1], 0, sizeof(throttle_data_[1]));

    // this could be faster by keeping running totals and
    //  subtracting [replace_idx_] before copying [0] into it,
    //  then adding new [replace_idx_].  But that needs more
    //  time for testing.
    for (loop=2; loop<THROTTLE_INTERVALS; ++loop)
    {
      tot_micros+=throttle_data_[loop]._micros;
      tot_keys+=throttle_data_[loop]._keys;
      tot_bytes+=throttle_data_[loop]._bytes;
      tot_compact+=throttle_data_[loop]._compactions;
    }   // for
  } // unique_lock

    // flag to skip throttle changes if zero data available
  no_data = (0 == tot_bytes && 0 == throttle_data_[0]._bytes);

  // reduce bytes by 10% for each excess level_0 files and/or excess write buffers
  adjustment_bytes = (tot_bytes*compaction_backlog)/10;
  if (adjustment_bytes<tot_bytes) {
    tot_bytes -= adjustment_bytes;
  } else {
    tot_bytes = 1;   // not zero, let smoothing drift number down instead of taking level-0
  }

  // lock thread_mutex_ while we update throttle_data_
  if (!no_data) {
    std::unique_lock<std::mutex> ul(thread_mutex_);

    // non-level0 data available?
    if (0!=tot_bytes && 0!=tot_micros.count())
    {
      // average bytes per secon for level 1+ compactions
      //  (adjust bytes upward by 1000000 since dividing by microseconds,
      //   yields integer bytes per second)
      new_throttle=((tot_bytes*1000000) / tot_micros.count());

    }   // if

    // attempt to most recent level0
    //  (only use most recent level0 until level1+ data becomes available,
    //   useful on restart of heavily loaded server)
    else if (0!=throttle_data_[0]._bytes && 0!=throttle_data_[0]._micros.count())
    {
      new_throttle=(throttle_data_[0]._bytes * 1000000) / throttle_data_[0]._micros.count();
    }   // else if
    else
    {
      new_throttle=1;
    }   // else

    if (0==new_throttle)
      new_throttle=1;     // throttle must have an effect

    // change the throttle slowly
    //  (+1 & +2 keep throttle moving toward goal when difference new and
    //   old is less than THROTTLE_SCALING)
    if (!first_throttle_) {
      temp_rate=throttle_bps_;

      if (temp_rate < new_throttle)
        temp_rate+=(new_throttle - temp_rate)/THROTTLE_SCALING +1;
      else
        temp_rate-=(temp_rate - new_throttle)/THROTTLE_SCALING +2;

      // +2 can make this go negative
      if (temp_rate<1)
        temp_rate=1;   // throttle must always have an effect

      throttle_bps_=temp_rate;

      // prepare for next interval
      memset(&throttle_data_[0], 0, sizeof(throttle_data_[0]));
    } else if (1<new_throttle) {
      // never had a valid throttle, and have first hint now
      throttle_bps_=new_throttle;

      first_throttle_=false;
    }  // else if

    SetThrottle();

  } // !no_data && unlock thread_mutex_

} // RocksDBThrottle::RecalculateThrottle


//
// Hack a throttle rate into the WriteController object
//
void RocksDBThrottle::SetThrottle() {
  // called by routine with thread_mutex_ held

  // this routine can get called before internalRocksDB_ is set
  if (nullptr != internalRocksDB_) {
    // inform write_controller_ of our new rate
    if (1<throttle_bps_) {
      // hard casting away of "const" ...
      delay_token_=(((WriteController&)internalRocksDB_->write_controller()).GetDelayToken(throttle_bps_));
    } else {
      delay_token_.reset();
    } // else
  } // if
} // RocksDBThrottle::SetThrottle


  /// @brief translate a (local) collection id into a collection name
//
// Use rocksdb's internal statistics to determine if
//  additional slowing of writes is warranted
//
int64_t RocksDBThrottle::ComputeBacklog() {
  int64_t compaction_backlog, imm_backlog, imm_trigger;
  bool ret_flag;
  std::string ret_string, property_name;
  int temp;

  // want count of level 0 files to estimate if compactions "behind"
  //  and therefore likely to start stalling / stopping
  compaction_backlog = 0;
  imm_backlog = 0;
  if (families_.size()) {
    imm_trigger = internalRocksDB_->GetOptions(families_[0]).max_write_buffer_number / 2;
  } else {
    imm_trigger = 3;
  } // else

  // loop through column families to obtain family specific counts
  for (auto & cf : families_) {
    property_name = rocksdb::DB::Properties::kNumFilesAtLevelPrefix;
    property_name.append("0");
    ret_flag=internalRocksDB_->GetProperty(cf, property_name, &ret_string);
    if (ret_flag) {
      temp=std::stoi(ret_string);
    } else {
      temp =0;
    } // else

    if (kL0_SlowdownWritesTrigger<=temp) {
      temp -= (kL0_SlowdownWritesTrigger -1);
    } else {
      temp = 0;
    } // else

    compaction_backlog += temp;

    property_name=rocksdb::DB::Properties::kNumImmutableMemTable;
    ret_flag=internalRocksDB_->GetProperty(cf, property_name, &ret_string);

    if (ret_flag) {
      temp=std::stoi(ret_string);
      imm_backlog += temp;
    } // if
  } // for

  if (imm_trigger<imm_backlog) {
    compaction_backlog += (imm_backlog - imm_trigger);
  } // if

  return compaction_backlog;
} // RocksDBThrottle::Computebacklog


/// @brief Adjust the active thread's priority to match the work
///  it is performing.  The routine is called HEAVILY.
void RocksDBThrottle::AdjustThreadPriority(int Adjustment) {
//#ifdef OS_LINUX
  // initialize thread infor if this the first time the thread has ever called
  if (!gThreadPriority._baseSet) {
    pid_t tid;
    int ret_val;

    tid = syscall(SYS_gettid);
    if (-1!=(int)tid)
    {
      errno=0;
      ret_val=getpriority(PRIO_PROCESS, tid);
      // ret_val could be -1 legally, so double test
      if (-1!=ret_val || 0==errno) {
        gThreadPriority._baseSet = true;
        gThreadPriority._basePriority = ret_val;
        gThreadPriority._currentPriority = ret_val;
      }   // if
    }   // if
  } // if

  // only change priorities if we
  if (gThreadPriority._baseSet
      && (gThreadPriority._basePriority+Adjustment)!=gThreadPriority._currentPriority) {

    pid_t tid;
    tid = syscall(SYS_gettid);
    if (-1!=(int)tid)
    {
      gThreadPriority._currentPriority = gThreadPriority._basePriority + Adjustment;
      setpriority(PRIO_PROCESS, tid, gThreadPriority._currentPriority);
    } // if
  } // if

//#endif
} // RocksDBThrottle::AdjustThreadPriority


} // namespace arangodb
