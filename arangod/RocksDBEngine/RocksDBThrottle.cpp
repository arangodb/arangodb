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

namespace arangodb {

// rocksdb flushes and compactions start and stop within same thread, no overlapping
thread_local std::chrono::steady_clock::time_point gFlushStart;


RocksDBEventListener::RocksDBEventListener()
  : internalRocksDB_(nullptr), thread_running_(false), throttle_bps_(0), first_throttle_(true)
{
  memset(&throttle_data_, 0, sizeof(throttle_data_));
}


RocksDBEventListener::~RocksDBEventListener() {

  if (thread_running_.load()) {
    thread_running_.store(false);

    thread_condvar_.notify_one();
    thread_future_.wait();
  } // if
}


void RocksDBEventListener::OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {

  // save start time in thread local storage
  gFlushStart = std::chrono::steady_clock::now();

  return;

} // RocksDBEventListener::OnFlushBegin


void RocksDBEventListener::OnFlushCompleted(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {
  std::chrono::microseconds flush_time;
  uint64_t flush_size;

  flush_time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - gFlushStart);
  flush_size = flush_job_info.table_properties.data_size + flush_job_info.table_properties.index_size
    + flush_job_info.table_properties.filter_size;

  SetThrottleWriteRate(flush_time, flush_job_info.table_properties.num_entries, flush_size, true);

  // start throttle after first data is posted
  //  (have seen some odd zero and small size flushes early)
  if (1024<flush_size) {
    std::once_flag init_flag_;

    std::call_once(init_flag_, &RocksDBEventListener::Startup, this, db);
  } // if

} // RocksDBEventListener::OnFlushCompleted


void RocksDBEventListener::OnCompactionCompleted(rocksdb::DB* db,
                                                 const rocksdb::CompactionJobInfo& ci) {

  std::chrono::microseconds elapsed(ci.stats.elapsed_micros);
  SetThrottleWriteRate(elapsed, ci.stats.num_output_records, ci.stats.total_output_bytes, false);

} // RocksDBEventListener::OnCompactionCompleted


void RocksDBEventListener::Startup(rocksdb::DB* db) {
  internalRocksDB_ = (rocksdb::DBImpl *)db;

  // addresses race condition during fast start/stop
  {
    std::unique_lock<std::mutex> ul(thread_mutex_);

    thread_future_ = std::async(std::launch::async, &RocksDBEventListener::ThreadLoop, this);

    while(!thread_running_.load())
      thread_condvar_.wait(ul);
  }   // mutex

} // RocksDBEventListener::Startup


void RocksDBEventListener::SetThrottleWriteRate(std::chrono::microseconds Micros,
                                                uint64_t Keys, uint64_t Bytes, bool IsLevel0) {
  // lock thread_mutex_ while we update throttle_data_
  std::unique_lock<std::mutex> ul(thread_mutex_);
  unsigned target_idx;

  // index 0 for level 0 compactions, index 1 for all others
  target_idx = (IsLevel0 ? 0 : 1);

  throttle_data_[target_idx].m_Micros+=Micros;
  throttle_data_[target_idx].m_Keys+=Keys;
  throttle_data_[target_idx].m_Bytes+=Bytes;
  throttle_data_[target_idx].m_Compactions+=1;

  // attempt to override throttle changes by rocksdb ... hammer this often
  //  (note that thread_mutex_ IS HELD)
  SetThrottle();

  return;
};


void RocksDBEventListener::ThreadLoop() {

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

} // RocksDBEventListener::ThreadLoop


//
// Routine to actually perform the throttle calculation,
//  now is external routing from ThreadLoop() to easy unit test
void RocksDBEventListener::RecalculateThrottle() {
  unsigned loop;
  std::chrono::microseconds tot_micros;
  uint64_t tot_bytes, tot_keys, tot_compact, adjustment_bytes;
  int64_t new_throttle, compaction_backlog;
  bool no_data;

  tot_micros*=0;
  tot_keys=0;
  tot_bytes=0;
  tot_compact=0;

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
      tot_micros+=throttle_data_[loop].m_Micros;
      tot_keys+=throttle_data_[loop].m_Keys;
      tot_bytes+=throttle_data_[loop].m_Bytes;
      tot_compact+=throttle_data_[loop].m_Compactions;
    }   // for
  } // unique_lock

    // flag to skip throttle changes if zero data available
  no_data = (0 == tot_bytes && 0 == throttle_data_[0].m_Bytes);

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
    else if (0!=throttle_data_[0].m_Bytes && 0!=throttle_data_[0].m_Micros.count())
    {
      new_throttle=(throttle_data_[0].m_Bytes * 1000000) / throttle_data_[0].m_Micros.count();
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
    int64_t temp_rate;

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

} // RocksDBEventListener::RecalculateThrottle


//
// Hack a throttle rate into the WriteController object
//
void RocksDBEventListener::SetThrottle() {
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
} // RocksDBEventListener::SetThrottle


//
// Use rocksdb's internal statistics to determine if
//  additional slowing of writes is warranted
//
int64_t RocksDBEventListener::ComputeBacklog() {
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
} // RocksDBEventListener::Computebacklog

} // namespace arangodb
