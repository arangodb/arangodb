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

#include <iostream>

namespace arangodb {

// rocksdb flushes and compactions start and stop within same thread, no overlapping
thread_local std::chrono::steady_clock::time_point gFlushStart;


RocksDBEventListener::RocksDBEventListener()
  : internalRocksDB_(nullptr), thread_running_(false), throttle_bps_(0), first_throttle_(true)
{}


RocksDBEventListener::~RocksDBEventListener() {
  thread_running_.store(false);

  thread_future_.wait();
}


void RocksDBEventListener::OnFlushBegin(rocksdb::DB* db, const rocksdb::FlushJobInfo& flush_job_info) {

  static std::once_flag init_flag;

  std::call_once(init_flag, &RocksDBEventListener::Startup, this, db);

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

} // RocksDBEventListener::OnFlushCompleted


void RocksDBEventListener::OnCompactionCompleted(rocksdb::DB* db,
                                                 const rocksdb::CompactionJobInfo& ci) {

  std::chrono::microseconds elapsed(ci.stats.elapsed_micros);
  SetThrottleWriteRate(elapsed, ci.stats.num_output_records, ci.stats.total_output_bytes, false);

} // RocksDBEventListener::OnCompactionCompleted


void RocksDBEventListener::Startup(rocksdb::DB* db) {
  internalRocksDB_ = (rocksdb::DBImpl *)db;

  memset(&throttle_data_, 0, sizeof(throttle_data_));
//  gThrottleRate=0;
//  gUnadjustedThrottleRate=0;

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

  std::cout << "SetThrottleWriteRate" << target_idx << Micros.count() << Keys << Bytes << IsLevel0 << std::endl;

  throttle_data_[target_idx].m_Micros+=Micros;
  throttle_data_[target_idx].m_Keys+=Keys;
  throttle_data_[target_idx].m_Bytes+=Bytes;
  throttle_data_[target_idx].m_Compactions+=1;

  return;
};


void RocksDBEventListener::ThreadLoop() {
  std::chrono::microseconds tot_micros;
  uint64_t tot_bytes, tot_keys, tot_compact;
  int64_t new_throttle;
  unsigned replace_idx, loop;

  replace_idx=2;

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
    {
      // lock thread_mutex_ while we update throttle_data_ and
      // wait on thread_condvar_
      std::unique_lock<std::mutex> ul(thread_mutex_);

      // sleep 1 minute
      std::chrono::seconds wait_seconds(THROTTLE_SECONDS);

      if (thread_running_.load()) { // test in case of race at shutdown
        thread_condvar_.wait_for(ul, wait_seconds);
      } //if

      throttle_data_[replace_idx]=throttle_data_[1];
      //throttle_data_[replace_idx].m_Backlog=0;
      memset(&throttle_data_[1], 0, sizeof(throttle_data_[1]));
    } // unlock thread_mutex_

    tot_micros*=0;
    tot_keys=0;
    tot_bytes=0;
    tot_compact=0;

    // this could be faster by keeping running totals and
    //  subtracting [replace_idx] before copying [0] into it,
    //  then adding new [replace_idx].  But that needs more
    //  time for testing.
    for (loop=2; loop<THROTTLE_INTERVALS; ++loop)
    {
      tot_micros+=throttle_data_[loop].m_Micros;
      tot_keys+=throttle_data_[loop].m_Keys;
      tot_bytes+=throttle_data_[loop].m_Bytes;
      tot_compact+=throttle_data_[loop].m_Compactions;
    }   // for

    // lock thread_mutex_ while we update throttle_data_
    {
      std::unique_lock<std::mutex> ul(thread_mutex_);

      // non-level0 data available?
      if (0!=tot_bytes && 0!=tot_micros.count())
      {
        // average bytes per secon for level 1+ compactions
        //  (adjust bytes upward by 1000000 since dividing by microseconds,
        //   yields integer bytes per second)
        new_throttle=((tot_bytes*1000000) / tot_micros.count());

        std::cout << "ThrottleLoop (normal)" << throttle_bps_ << new_throttle << std::endl;

      }   // if

      // attempt to most recent level0
      //  (only use most recent level0 until level1+ data becomes available,
      //   useful on restart of heavily loaded server)
      else if (0!=throttle_data_[0].m_Bytes && 0!=throttle_data_[0].m_Micros.count())
      {
        new_throttle=(throttle_data_[0].m_Bytes * 1000000) / throttle_data_[0].m_Micros.count();
        std::cout << "ThrottleLoop (level 0)" << throttle_bps_ << new_throttle << std::endl;

      }   // else if
      else
      {
        new_throttle=1;
        std::cout << "ThrottleLoop (default)" << throttle_bps_ << new_throttle << std::endl;
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

        std::cout << "ThrottleLoop (smoothing)" << throttle_bps_ << temp_rate << std::endl;

        throttle_bps_=temp_rate;

        // Log(NULL, "ThrottleRate %" PRIu64 ", UnadjustedThrottleRate %" PRIu64, gThrottleRate, gUnadjustedThrottleRate);

        // prepare for next interval
        memset(&throttle_data_[0], 0, sizeof(throttle_data_[0]));
      } else if (1<new_throttle) {
        // never had a valid throttle, and have first hint now
        throttle_bps_=new_throttle;

        std::cout << "ThrottleLoop (first throttle)" << throttle_bps_ << new_throttle << std::endl;

        first_throttle_=false;
      }  // else if
    } // unlock thread_mutex_

    // inform write_controller_ of our new rate
    if (1<throttle_bps_) {
      // hard casting away of "const" ...
      delay_token_=(((WriteController&)internalRocksDB_->write_controller()).GetDelayToken(throttle_bps_));
    } else {
      delay_token_.reset();
    } // else

    ++replace_idx;
        if (THROTTLE_INTERVALS==replace_idx)
            replace_idx=2;

  } // while

} // RocksDBEventListener::ThreadLoop

#if 0

// current time, on roughly a 60 second scale
//  (used to reduce number of OS calls for expiry)
uint64_t gCurrentTime=0;

uint64_t gThrottleRate, gUnadjustedThrottleRate;

static volatile bool gThrottleRunning=false;

void RocksDBThrottle::ThrottleThread() {
    time_t now_seconds, cache_expire;
    struct timespec wait_time;

    now_seconds=0;
    cache_expire=0;
    new_unadjusted=1;

    while(gThrottleRunning)
    {
        //
        // This is code polls for existance of /etc/riak/perf_counters and sets
        //  the global gPerfCountersDisabled accordingly.
        //  Sure, there should be a better place for this code.  But fits here nicely today.
        //
        ret_val=access("/etc/riak/perf_counters", F_OK);
        gPerfCountersDisabled=(-1==ret_val);


        //
        // This is code to manage / flush the flexcache's old file cache entries.
        //  Sure, there should be a better place for this code.  But fits here nicely today.
        //
        if (cache_expire < now_seconds)
        {
            cache_expire = now_seconds + 60*60;  // hard coded to one hour for now
            DBList()->ScanDBs(true,  &DBImpl::PurgeExpiredFileCache);
            DBList()->ScanDBs(false, &DBImpl::PurgeExpiredFileCache);
        }   // if

        //
        // This is a second non-throttle task added to this one minute loop.  Pattern forming.
        //  See if hot backup wants to initiate.
        //
	CheckHotBackupTrigger();

        // nudge compaction logic of potential grooming
        if (0==gCompactionThreads->m_WorkQueueAtomic)  // user databases
            DBList()->ScanDBs(false, &DBImpl::CheckAvailableCompactions);
        if (0==gCompactionThreads->m_WorkQueueAtomic)  // internal databases
            DBList()->ScanDBs(true,  &DBImpl::CheckAvailableCompactions);

    }   // while

    return;

} // RocksDBThrottle::ThrottleThread



uint64_t GetThrottleWriteRate() {return(gThrottleRate);};
uint64_t GetUnadjustedThrottleWriteRate() {return(gUnadjustedThrottleRate);};

// clock_gettime but only updated once every 60 seconds (roughly)
uint64_t GetCachedTimeMicros() {return(gCurrentTime);};
void SetCachedTimeMicros(uint64_t Time) {gCurrentTime=Time;};
/**
 * ThrottleStopThreads() is the first step in a two step shutdown.
 * This stops the 1 minute throttle calculation loop that also
 * can initiate leveldb compaction actions.  Background compaction
 * threads should stop between these two steps.
 */
void ThrottleStopThreads()
{
    if (gThrottleRunning)
    {
        gThrottleRunning=false;

        // lock thread_mutex_ so that we can signal thread_condvar_
        {
            MutexLock lock(thread_mutex_);
            thread_condvar_->Signal();
        } // unlock thread_mutex_

        pthread_join(gThrottleThreadId, NULL);
    }   // if

    return;

}   // ThrottleShutdown

/**
 * ThrottleClose is the second step in a two step shutdown of
 *  throttle.  The intent is for background compaction threads
 *  to stop between these two steps.
 */
void ThrottleClose()
{
    // safety check
    if (gThrottleRunning)
        ThrottleStopThreads();

    return;
}   // ThrottleShutdown
#endif

} // namespace arangodb
