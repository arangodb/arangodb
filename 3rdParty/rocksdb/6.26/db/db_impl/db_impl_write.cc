//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
#include <cinttypes>

#include "db/db_impl/db_impl.h"
#include "db/error_handler.h"
#include "db/event_helpers.h"
#include "logging/logging.h"
#include "monitoring/perf_context_imp.h"
#include "options/options_helper.h"
#include "test_util/sync_point.h"
#include "util/cast_util.h"

namespace ROCKSDB_NAMESPACE {
// Convenience methods
Status DBImpl::Put(const WriteOptions& o, ColumnFamilyHandle* column_family,
                   const Slice& key, const Slice& val) {
  return DB::Put(o, column_family, key, val);
}

Status DBImpl::Merge(const WriteOptions& o, ColumnFamilyHandle* column_family,
                     const Slice& key, const Slice& val) {
  auto cfh = static_cast_with_check<ColumnFamilyHandleImpl>(column_family);
  if (!cfh->cfd()->ioptions()->merge_operator) {
    return Status::NotSupported("Provide a merge_operator when opening DB");
  } else {
    return DB::Merge(o, column_family, key, val);
  }
}

Status DBImpl::Delete(const WriteOptions& write_options,
                      ColumnFamilyHandle* column_family, const Slice& key) {
  return DB::Delete(write_options, column_family, key);
}

Status DBImpl::SingleDelete(const WriteOptions& write_options,
                            ColumnFamilyHandle* column_family,
                            const Slice& key) {
  return DB::SingleDelete(write_options, column_family, key);
}

void DBImpl::SetRecoverableStatePreReleaseCallback(
    PreReleaseCallback* callback) {
  recoverable_state_pre_release_callback_.reset(callback);
}

Status DBImpl::Write(const WriteOptions& write_options, WriteBatch* my_batch) {
  return WriteImpl(write_options, my_batch, nullptr, nullptr);
}

#ifndef ROCKSDB_LITE
Status DBImpl::WriteWithCallback(const WriteOptions& write_options,
                                 WriteBatch* my_batch,
                                 WriteCallback* callback) {
  return WriteImpl(write_options, my_batch, callback, nullptr);
}
#endif  // ROCKSDB_LITE

// The main write queue. This is the only write queue that updates LastSequence.
// When using one write queue, the same sequence also indicates the last
// published sequence.
Status DBImpl::WriteImpl(const WriteOptions& write_options,
                         WriteBatch* my_batch, WriteCallback* callback,
                         uint64_t* log_used, uint64_t log_ref,
                         bool disable_memtable, uint64_t* seq_used,
                         size_t batch_cnt,
                         PreReleaseCallback* pre_release_callback) {
  assert(!seq_per_batch_ || batch_cnt != 0);
  if (my_batch == nullptr) {
    return Status::Corruption("Batch is nullptr!");
  }
  if (tracer_) {
    InstrumentedMutexLock lock(&trace_mutex_);
    if (tracer_) {
      // TODO: maybe handle the tracing status?
      tracer_->Write(my_batch).PermitUncheckedError();
    }
  }
  if (write_options.sync && write_options.disableWAL) {
    return Status::InvalidArgument("Sync writes has to enable WAL.");
  }
  if (two_write_queues_ && immutable_db_options_.enable_pipelined_write) {
    return Status::NotSupported(
        "pipelined_writes is not compatible with concurrent prepares");
  }
  if (seq_per_batch_ && immutable_db_options_.enable_pipelined_write) {
    // TODO(yiwu): update pipeline write with seq_per_batch and batch_cnt
    return Status::NotSupported(
        "pipelined_writes is not compatible with seq_per_batch");
  }
  if (immutable_db_options_.unordered_write &&
      immutable_db_options_.enable_pipelined_write) {
    return Status::NotSupported(
        "pipelined_writes is not compatible with unordered_write");
  }
  // Otherwise IsLatestPersistentState optimization does not make sense
  assert(!WriteBatchInternal::IsLatestPersistentState(my_batch) ||
         disable_memtable);

  if (write_options.low_pri) {
    Status s = ThrottleLowPriWritesIfNeeded(write_options, my_batch);
    if (!s.ok()) {
      return s;
    }
  }

  if (two_write_queues_ && disable_memtable) {
    AssignOrder assign_order =
        seq_per_batch_ ? kDoAssignOrder : kDontAssignOrder;
    // Otherwise it is WAL-only Prepare batches in WriteCommitted policy and
    // they don't consume sequence.
    return WriteImplWALOnly(&nonmem_write_thread_, write_options, my_batch,
                            callback, log_used, log_ref, seq_used, batch_cnt,
                            pre_release_callback, assign_order,
                            kDontPublishLastSeq, disable_memtable);
  }

  if (immutable_db_options_.unordered_write) {
    const size_t sub_batch_cnt = batch_cnt != 0
                                     ? batch_cnt
                                     // every key is a sub-batch consuming a seq
                                     : WriteBatchInternal::Count(my_batch);
    uint64_t seq = 0;
    // Use a write thread to i) optimize for WAL write, ii) publish last
    // sequence in in increasing order, iii) call pre_release_callback serially
    Status status = WriteImplWALOnly(
        &write_thread_, write_options, my_batch, callback, log_used, log_ref,
        &seq, sub_batch_cnt, pre_release_callback, kDoAssignOrder,
        kDoPublishLastSeq, disable_memtable);
    TEST_SYNC_POINT("DBImpl::WriteImpl:UnorderedWriteAfterWriteWAL");
    if (!status.ok()) {
      return status;
    }
    if (seq_used) {
      *seq_used = seq;
    }
    if (!disable_memtable) {
      TEST_SYNC_POINT("DBImpl::WriteImpl:BeforeUnorderedWriteMemtable");
      status = UnorderedWriteMemtable(write_options, my_batch, callback,
                                      log_ref, seq, sub_batch_cnt);
    }
    return status;
  }

  if (immutable_db_options_.enable_pipelined_write) {
    return PipelinedWriteImpl(write_options, my_batch, callback, log_used,
                              log_ref, disable_memtable, seq_used);
  }

  PERF_TIMER_GUARD(write_pre_and_post_process_time);
  WriteThread::Writer w(write_options, my_batch, callback, log_ref,
                        disable_memtable, batch_cnt, pre_release_callback);

  if (!write_options.disableWAL) {
    RecordTick(stats_, WRITE_WITH_WAL);
  }

  StopWatch write_sw(immutable_db_options_.clock, stats_, DB_WRITE);

  write_thread_.JoinBatchGroup(&w);
  if (w.state == WriteThread::STATE_PARALLEL_MEMTABLE_WRITER) {
    // we are a non-leader in a parallel group

    if (w.ShouldWriteToMemtable()) {
      PERF_TIMER_STOP(write_pre_and_post_process_time);
      PERF_TIMER_GUARD(write_memtable_time);

      ColumnFamilyMemTablesImpl column_family_memtables(
          versions_->GetColumnFamilySet());
      w.status = WriteBatchInternal::InsertInto(
          &w, w.sequence, &column_family_memtables, &flush_scheduler_,
          &trim_history_scheduler_,
          write_options.ignore_missing_column_families, 0 /*log_number*/, this,
          true /*concurrent_memtable_writes*/, seq_per_batch_, w.batch_cnt,
          batch_per_txn_, write_options.memtable_insert_hint_per_batch);

      PERF_TIMER_START(write_pre_and_post_process_time);
    }

    if (write_thread_.CompleteParallelMemTableWriter(&w)) {
      // we're responsible for exit batch group
      // TODO(myabandeh): propagate status to write_group
      auto last_sequence = w.write_group->last_sequence;
      versions_->SetLastSequence(last_sequence);
      MemTableInsertStatusCheck(w.status);
      write_thread_.ExitAsBatchGroupFollower(&w);
    }
    assert(w.state == WriteThread::STATE_COMPLETED);
    // STATE_COMPLETED conditional below handles exit
  }
  if (w.state == WriteThread::STATE_COMPLETED) {
    if (log_used != nullptr) {
      *log_used = w.log_used;
    }
    if (seq_used != nullptr) {
      *seq_used = w.sequence;
    }
    // write is complete and leader has updated sequence
    return w.FinalStatus();
  }
  // else we are the leader of the write batch group
  assert(w.state == WriteThread::STATE_GROUP_LEADER);
  Status status;
  // Once reaches this point, the current writer "w" will try to do its write
  // job.  It may also pick up some of the remaining writers in the "writers_"
  // when it finds suitable, and finish them in the same write batch.
  // This is how a write job could be done by the other writer.
  WriteContext write_context;
  WriteThread::WriteGroup write_group;
  bool in_parallel_group = false;
  uint64_t last_sequence = kMaxSequenceNumber;

  mutex_.Lock();

  bool need_log_sync = write_options.sync;
  bool need_log_dir_sync = need_log_sync && !log_dir_synced_;
  assert(!two_write_queues_ || !disable_memtable);
  {
    // With concurrent writes we do preprocess only in the write thread that
    // also does write to memtable to avoid sync issue on shared data structure
    // with the other thread

    // PreprocessWrite does its own perf timing.
    PERF_TIMER_STOP(write_pre_and_post_process_time);

    status = PreprocessWrite(write_options, &need_log_sync, &write_context);
    if (!two_write_queues_) {
      // Assign it after ::PreprocessWrite since the sequence might advance
      // inside it by WriteRecoverableState
      last_sequence = versions_->LastSequence();
    }

    PERF_TIMER_START(write_pre_and_post_process_time);
  }
  log::Writer* log_writer = logs_.back().writer;

  mutex_.Unlock();

  // Add to log and apply to memtable.  We can release the lock
  // during this phase since &w is currently responsible for logging
  // and protects against concurrent loggers and concurrent writes
  // into memtables

  TEST_SYNC_POINT("DBImpl::WriteImpl:BeforeLeaderEnters");
  last_batch_group_size_ =
      write_thread_.EnterAsBatchGroupLeader(&w, &write_group);

  IOStatus io_s;
  Status pre_release_cb_status;
  if (status.ok()) {
    // Rules for when we can update the memtable concurrently
    // 1. supported by memtable
    // 2. Puts are not okay if inplace_update_support
    // 3. Merges are not okay
    //
    // Rules 1..2 are enforced by checking the options
    // during startup (CheckConcurrentWritesSupported), so if
    // options.allow_concurrent_memtable_write is true then they can be
    // assumed to be true.  Rule 3 is checked for each batch.  We could
    // relax rules 2 if we could prevent write batches from referring
    // more than once to a particular key.
    bool parallel = immutable_db_options_.allow_concurrent_memtable_write &&
                    write_group.size > 1;
    size_t total_count = 0;
    size_t valid_batches = 0;
    size_t total_byte_size = 0;
    size_t pre_release_callback_cnt = 0;
    for (auto* writer : write_group) {
      if (writer->CheckCallback(this)) {
        valid_batches += writer->batch_cnt;
        if (writer->ShouldWriteToMemtable()) {
          total_count += WriteBatchInternal::Count(writer->batch);
          parallel = parallel && !writer->batch->HasMerge();
        }
        total_byte_size = WriteBatchInternal::AppendedByteSize(
            total_byte_size, WriteBatchInternal::ByteSize(writer->batch));
        if (writer->pre_release_callback) {
          pre_release_callback_cnt++;
        }
      }
    }
    // Note about seq_per_batch_: either disableWAL is set for the entire write
    // group or not. In either case we inc seq for each write batch with no
    // failed callback. This means that there could be a batch with
    // disalbe_memtable in between; although we do not write this batch to
    // memtable it still consumes a seq. Otherwise, if !seq_per_batch_, we inc
    // the seq per valid written key to mem.
    size_t seq_inc = seq_per_batch_ ? valid_batches : total_count;

    const bool concurrent_update = two_write_queues_;
    // Update stats while we are an exclusive group leader, so we know
    // that nobody else can be writing to these particular stats.
    // We're optimistic, updating the stats before we successfully
    // commit.  That lets us release our leader status early.
    auto stats = default_cf_internal_stats_;
    stats->AddDBStats(InternalStats::kIntStatsNumKeysWritten, total_count,
                      concurrent_update);
    RecordTick(stats_, NUMBER_KEYS_WRITTEN, total_count);
    stats->AddDBStats(InternalStats::kIntStatsBytesWritten, total_byte_size,
                      concurrent_update);
    RecordTick(stats_, BYTES_WRITTEN, total_byte_size);
    stats->AddDBStats(InternalStats::kIntStatsWriteDoneBySelf, 1,
                      concurrent_update);
    RecordTick(stats_, WRITE_DONE_BY_SELF);
    auto write_done_by_other = write_group.size - 1;
    if (write_done_by_other > 0) {
      stats->AddDBStats(InternalStats::kIntStatsWriteDoneByOther,
                        write_done_by_other, concurrent_update);
      RecordTick(stats_, WRITE_DONE_BY_OTHER, write_done_by_other);
    }
    RecordInHistogram(stats_, BYTES_PER_WRITE, total_byte_size);

    if (write_options.disableWAL) {
      has_unpersisted_data_.store(true, std::memory_order_relaxed);
    }

    PERF_TIMER_STOP(write_pre_and_post_process_time);

    if (!two_write_queues_) {
      if (status.ok() && !write_options.disableWAL) {
        PERF_TIMER_GUARD(write_wal_time);
        io_s = WriteToWAL(write_group, log_writer, log_used, need_log_sync,
                          need_log_dir_sync, last_sequence + 1);
      }
    } else {
      if (status.ok() && !write_options.disableWAL) {
        PERF_TIMER_GUARD(write_wal_time);
        // LastAllocatedSequence is increased inside WriteToWAL under
        // wal_write_mutex_ to ensure ordered events in WAL
        io_s = ConcurrentWriteToWAL(write_group, log_used, &last_sequence,
                                    seq_inc);
      } else {
        // Otherwise we inc seq number for memtable writes
        last_sequence = versions_->FetchAddLastAllocatedSequence(seq_inc);
      }
    }
    status = io_s;
    assert(last_sequence != kMaxSequenceNumber);
    const SequenceNumber current_sequence = last_sequence + 1;
    last_sequence += seq_inc;

    // PreReleaseCallback is called after WAL write and before memtable write
    if (status.ok()) {
      SequenceNumber next_sequence = current_sequence;
      size_t index = 0;
      // Note: the logic for advancing seq here must be consistent with the
      // logic in WriteBatchInternal::InsertInto(write_group...) as well as
      // with WriteBatchInternal::InsertInto(write_batch...) that is called on
      // the merged batch during recovery from the WAL.
      for (auto* writer : write_group) {
        if (writer->CallbackFailed()) {
          continue;
        }
        writer->sequence = next_sequence;
        if (writer->pre_release_callback) {
          Status ws = writer->pre_release_callback->Callback(
              writer->sequence, disable_memtable, writer->log_used, index++,
              pre_release_callback_cnt);
          if (!ws.ok()) {
            status = pre_release_cb_status = ws;
            break;
          }
        }
        if (seq_per_batch_) {
          assert(writer->batch_cnt);
          next_sequence += writer->batch_cnt;
        } else if (writer->ShouldWriteToMemtable()) {
          next_sequence += WriteBatchInternal::Count(writer->batch);
        }
      }
    }

    if (status.ok()) {
      PERF_TIMER_GUARD(write_memtable_time);

      if (!parallel) {
        // w.sequence will be set inside InsertInto
        w.status = WriteBatchInternal::InsertInto(
            write_group, current_sequence, column_family_memtables_.get(),
            &flush_scheduler_, &trim_history_scheduler_,
            write_options.ignore_missing_column_families,
            0 /*recovery_log_number*/, this, parallel, seq_per_batch_,
            batch_per_txn_);
      } else {
        write_group.last_sequence = last_sequence;
        write_thread_.LaunchParallelMemTableWriters(&write_group);
        in_parallel_group = true;

        // Each parallel follower is doing each own writes. The leader should
        // also do its own.
        if (w.ShouldWriteToMemtable()) {
          ColumnFamilyMemTablesImpl column_family_memtables(
              versions_->GetColumnFamilySet());
          assert(w.sequence == current_sequence);
          w.status = WriteBatchInternal::InsertInto(
              &w, w.sequence, &column_family_memtables, &flush_scheduler_,
              &trim_history_scheduler_,
              write_options.ignore_missing_column_families, 0 /*log_number*/,
              this, true /*concurrent_memtable_writes*/, seq_per_batch_,
              w.batch_cnt, batch_per_txn_,
              write_options.memtable_insert_hint_per_batch);
        }
      }
      if (seq_used != nullptr) {
        *seq_used = w.sequence;
      }
    }
  }
  PERF_TIMER_START(write_pre_and_post_process_time);

  if (!w.CallbackFailed()) {
    if (!io_s.ok()) {
      assert(pre_release_cb_status.ok());
      IOStatusCheck(io_s);
    } else {
      WriteStatusCheck(pre_release_cb_status);
    }
  } else {
    assert(io_s.ok() && pre_release_cb_status.ok());
  }

  if (need_log_sync) {
    mutex_.Lock();
    if (status.ok()) {
      status = MarkLogsSynced(logfile_number_, need_log_dir_sync);
    } else {
      MarkLogsNotSynced(logfile_number_);
    }
    mutex_.Unlock();
    // Requesting sync with two_write_queues_ is expected to be very rare. We
    // hence provide a simple implementation that is not necessarily efficient.
    if (two_write_queues_) {
      if (manual_wal_flush_) {
        status = FlushWAL(true);
      } else {
        status = SyncWAL();
      }
    }
  }

  bool should_exit_batch_group = true;
  if (in_parallel_group) {
    // CompleteParallelWorker returns true if this thread should
    // handle exit, false means somebody else did
    should_exit_batch_group = write_thread_.CompleteParallelMemTableWriter(&w);
  }
  if (should_exit_batch_group) {
    if (status.ok()) {
      // Note: if we are to resume after non-OK statuses we need to revisit how
      // we reacts to non-OK statuses here.
      versions_->SetLastSequence(last_sequence);
    }
    MemTableInsertStatusCheck(w.status);
    write_thread_.ExitAsBatchGroupLeader(write_group, status);
  }

  if (status.ok()) {
    status = w.FinalStatus();
  }
  return status;
}

Status DBImpl::PipelinedWriteImpl(const WriteOptions& write_options,
                                  WriteBatch* my_batch, WriteCallback* callback,
                                  uint64_t* log_used, uint64_t log_ref,
                                  bool disable_memtable, uint64_t* seq_used) {
  PERF_TIMER_GUARD(write_pre_and_post_process_time);
  StopWatch write_sw(immutable_db_options_.clock, stats_, DB_WRITE);

  WriteContext write_context;

  WriteThread::Writer w(write_options, my_batch, callback, log_ref,
                        disable_memtable);
  write_thread_.JoinBatchGroup(&w);
  TEST_SYNC_POINT("DBImplWrite::PipelinedWriteImpl:AfterJoinBatchGroup");
  if (w.state == WriteThread::STATE_GROUP_LEADER) {
    WriteThread::WriteGroup wal_write_group;
    if (w.callback && !w.callback->AllowWriteBatching()) {
      write_thread_.WaitForMemTableWriters();
    }
    mutex_.Lock();
    bool need_log_sync = !write_options.disableWAL && write_options.sync;
    bool need_log_dir_sync = need_log_sync && !log_dir_synced_;
    // PreprocessWrite does its own perf timing.
    PERF_TIMER_STOP(write_pre_and_post_process_time);
    w.status = PreprocessWrite(write_options, &need_log_sync, &write_context);
    PERF_TIMER_START(write_pre_and_post_process_time);
    log::Writer* log_writer = logs_.back().writer;
    mutex_.Unlock();

    // This can set non-OK status if callback fail.
    last_batch_group_size_ =
        write_thread_.EnterAsBatchGroupLeader(&w, &wal_write_group);
    const SequenceNumber current_sequence =
        write_thread_.UpdateLastSequence(versions_->LastSequence()) + 1;
    size_t total_count = 0;
    size_t total_byte_size = 0;

    if (w.status.ok()) {
      SequenceNumber next_sequence = current_sequence;
      for (auto writer : wal_write_group) {
        if (writer->CheckCallback(this)) {
          if (writer->ShouldWriteToMemtable()) {
            writer->sequence = next_sequence;
            size_t count = WriteBatchInternal::Count(writer->batch);
            next_sequence += count;
            total_count += count;
          }
          total_byte_size = WriteBatchInternal::AppendedByteSize(
              total_byte_size, WriteBatchInternal::ByteSize(writer->batch));
        }
      }
      if (w.disable_wal) {
        has_unpersisted_data_.store(true, std::memory_order_relaxed);
      }
      write_thread_.UpdateLastSequence(current_sequence + total_count - 1);
    }

    auto stats = default_cf_internal_stats_;
    stats->AddDBStats(InternalStats::kIntStatsNumKeysWritten, total_count);
    RecordTick(stats_, NUMBER_KEYS_WRITTEN, total_count);
    stats->AddDBStats(InternalStats::kIntStatsBytesWritten, total_byte_size);
    RecordTick(stats_, BYTES_WRITTEN, total_byte_size);
    RecordInHistogram(stats_, BYTES_PER_WRITE, total_byte_size);

    PERF_TIMER_STOP(write_pre_and_post_process_time);

    IOStatus io_s;
    io_s.PermitUncheckedError();  // Allow io_s to be uninitialized

    if (w.status.ok() && !write_options.disableWAL) {
      PERF_TIMER_GUARD(write_wal_time);
      stats->AddDBStats(InternalStats::kIntStatsWriteDoneBySelf, 1);
      RecordTick(stats_, WRITE_DONE_BY_SELF, 1);
      if (wal_write_group.size > 1) {
        stats->AddDBStats(InternalStats::kIntStatsWriteDoneByOther,
                          wal_write_group.size - 1);
        RecordTick(stats_, WRITE_DONE_BY_OTHER, wal_write_group.size - 1);
      }
      io_s = WriteToWAL(wal_write_group, log_writer, log_used, need_log_sync,
                        need_log_dir_sync, current_sequence);
      w.status = io_s;
    }

    if (!w.CallbackFailed()) {
      if (!io_s.ok()) {
        IOStatusCheck(io_s);
      } else {
        WriteStatusCheck(w.status);
      }
    }

    if (need_log_sync) {
      mutex_.Lock();
      if (w.status.ok()) {
        w.status = MarkLogsSynced(logfile_number_, need_log_dir_sync);
      } else {
        MarkLogsNotSynced(logfile_number_);
      }
      mutex_.Unlock();
    }

    write_thread_.ExitAsBatchGroupLeader(wal_write_group, w.status);
  }

  // NOTE: the memtable_write_group is declared before the following
  // `if` statement because its lifetime needs to be longer
  // that the inner context  of the `if` as a reference to it
  // may be used further below within the outer _write_thread
  WriteThread::WriteGroup memtable_write_group;

  if (w.state == WriteThread::STATE_MEMTABLE_WRITER_LEADER) {
    PERF_TIMER_GUARD(write_memtable_time);
    assert(w.ShouldWriteToMemtable());
    write_thread_.EnterAsMemTableWriter(&w, &memtable_write_group);
    if (memtable_write_group.size > 1 &&
        immutable_db_options_.allow_concurrent_memtable_write) {
      write_thread_.LaunchParallelMemTableWriters(&memtable_write_group);
    } else {
      memtable_write_group.status = WriteBatchInternal::InsertInto(
          memtable_write_group, w.sequence, column_family_memtables_.get(),
          &flush_scheduler_, &trim_history_scheduler_,
          write_options.ignore_missing_column_families, 0 /*log_number*/, this,
          false /*concurrent_memtable_writes*/, seq_per_batch_, batch_per_txn_);
      versions_->SetLastSequence(memtable_write_group.last_sequence);
      write_thread_.ExitAsMemTableWriter(&w, memtable_write_group);
    }
  } else {
    // NOTE: the memtable_write_group is never really used,
    // so we need to set its status to pass ASSERT_STATUS_CHECKED
    memtable_write_group.status.PermitUncheckedError();
  }

  if (w.state == WriteThread::STATE_PARALLEL_MEMTABLE_WRITER) {
    assert(w.ShouldWriteToMemtable());
    ColumnFamilyMemTablesImpl column_family_memtables(
        versions_->GetColumnFamilySet());
    w.status = WriteBatchInternal::InsertInto(
        &w, w.sequence, &column_family_memtables, &flush_scheduler_,
        &trim_history_scheduler_, write_options.ignore_missing_column_families,
        0 /*log_number*/, this, true /*concurrent_memtable_writes*/,
        false /*seq_per_batch*/, 0 /*batch_cnt*/, true /*batch_per_txn*/,
        write_options.memtable_insert_hint_per_batch);
    if (write_thread_.CompleteParallelMemTableWriter(&w)) {
      MemTableInsertStatusCheck(w.status);
      versions_->SetLastSequence(w.write_group->last_sequence);
      write_thread_.ExitAsMemTableWriter(&w, *w.write_group);
    }
  }
  if (seq_used != nullptr) {
    *seq_used = w.sequence;
  }

  assert(w.state == WriteThread::STATE_COMPLETED);
  return w.FinalStatus();
}

Status DBImpl::UnorderedWriteMemtable(const WriteOptions& write_options,
                                      WriteBatch* my_batch,
                                      WriteCallback* callback, uint64_t log_ref,
                                      SequenceNumber seq,
                                      const size_t sub_batch_cnt) {
  PERF_TIMER_GUARD(write_pre_and_post_process_time);
  StopWatch write_sw(immutable_db_options_.clock, stats_, DB_WRITE);

  WriteThread::Writer w(write_options, my_batch, callback, log_ref,
                        false /*disable_memtable*/);

  if (w.CheckCallback(this) && w.ShouldWriteToMemtable()) {
    w.sequence = seq;
    size_t total_count = WriteBatchInternal::Count(my_batch);
    InternalStats* stats = default_cf_internal_stats_;
    stats->AddDBStats(InternalStats::kIntStatsNumKeysWritten, total_count);
    RecordTick(stats_, NUMBER_KEYS_WRITTEN, total_count);

    ColumnFamilyMemTablesImpl column_family_memtables(
        versions_->GetColumnFamilySet());
    w.status = WriteBatchInternal::InsertInto(
        &w, w.sequence, &column_family_memtables, &flush_scheduler_,
        &trim_history_scheduler_, write_options.ignore_missing_column_families,
        0 /*log_number*/, this, true /*concurrent_memtable_writes*/,
        seq_per_batch_, sub_batch_cnt, true /*batch_per_txn*/,
        write_options.memtable_insert_hint_per_batch);
    if (write_options.disableWAL) {
      has_unpersisted_data_.store(true, std::memory_order_relaxed);
    }
  }

  size_t pending_cnt = pending_memtable_writes_.fetch_sub(1) - 1;
  if (pending_cnt == 0) {
    // switch_cv_ waits until pending_memtable_writes_ = 0. Locking its mutex
    // before notify ensures that cv is in waiting state when it is notified
    // thus not missing the update to pending_memtable_writes_ even though it is
    // not modified under the mutex.
    std::lock_guard<std::mutex> lck(switch_mutex_);
    switch_cv_.notify_all();
  }
  WriteStatusCheck(w.status);

  if (!w.FinalStatus().ok()) {
    return w.FinalStatus();
  }
  return Status::OK();
}

// The 2nd write queue. If enabled it will be used only for WAL-only writes.
// This is the only queue that updates LastPublishedSequence which is only
// applicable in a two-queue setting.
Status DBImpl::WriteImplWALOnly(
    WriteThread* write_thread, const WriteOptions& write_options,
    WriteBatch* my_batch, WriteCallback* callback, uint64_t* log_used,
    const uint64_t log_ref, uint64_t* seq_used, const size_t sub_batch_cnt,
    PreReleaseCallback* pre_release_callback, const AssignOrder assign_order,
    const PublishLastSeq publish_last_seq, const bool disable_memtable) {
  PERF_TIMER_GUARD(write_pre_and_post_process_time);
  WriteThread::Writer w(write_options, my_batch, callback, log_ref,
                        disable_memtable, sub_batch_cnt, pre_release_callback);
  RecordTick(stats_, WRITE_WITH_WAL);
  StopWatch write_sw(immutable_db_options_.clock, stats_, DB_WRITE);

  write_thread->JoinBatchGroup(&w);
  assert(w.state != WriteThread::STATE_PARALLEL_MEMTABLE_WRITER);
  if (w.state == WriteThread::STATE_COMPLETED) {
    if (log_used != nullptr) {
      *log_used = w.log_used;
    }
    if (seq_used != nullptr) {
      *seq_used = w.sequence;
    }
    return w.FinalStatus();
  }
  // else we are the leader of the write batch group
  assert(w.state == WriteThread::STATE_GROUP_LEADER);

  if (publish_last_seq == kDoPublishLastSeq) {
    Status status;

    // Currently we only use kDoPublishLastSeq in unordered_write
    assert(immutable_db_options_.unordered_write);
    WriteContext write_context;
    if (error_handler_.IsDBStopped()) {
      status = error_handler_.GetBGError();
    }
    // TODO(myabandeh): Make preliminary checks thread-safe so we could do them
    // without paying the cost of obtaining the mutex.
    if (status.ok()) {
      InstrumentedMutexLock l(&mutex_);
      bool need_log_sync = false;
      status = PreprocessWrite(write_options, &need_log_sync, &write_context);
      WriteStatusCheckOnLocked(status);
    }
    if (!status.ok()) {
      WriteThread::WriteGroup write_group;
      write_thread->EnterAsBatchGroupLeader(&w, &write_group);
      write_thread->ExitAsBatchGroupLeader(write_group, status);
      return status;
    }
  }

  WriteThread::WriteGroup write_group;
  uint64_t last_sequence;
  write_thread->EnterAsBatchGroupLeader(&w, &write_group);
  // Note: no need to update last_batch_group_size_ here since the batch writes
  // to WAL only

  size_t pre_release_callback_cnt = 0;
  size_t total_byte_size = 0;
  for (auto* writer : write_group) {
    if (writer->CheckCallback(this)) {
      total_byte_size = WriteBatchInternal::AppendedByteSize(
          total_byte_size, WriteBatchInternal::ByteSize(writer->batch));
      if (writer->pre_release_callback) {
        pre_release_callback_cnt++;
      }
    }
  }

  const bool concurrent_update = true;
  // Update stats while we are an exclusive group leader, so we know
  // that nobody else can be writing to these particular stats.
  // We're optimistic, updating the stats before we successfully
  // commit.  That lets us release our leader status early.
  auto stats = default_cf_internal_stats_;
  stats->AddDBStats(InternalStats::kIntStatsBytesWritten, total_byte_size,
                    concurrent_update);
  RecordTick(stats_, BYTES_WRITTEN, total_byte_size);
  stats->AddDBStats(InternalStats::kIntStatsWriteDoneBySelf, 1,
                    concurrent_update);
  RecordTick(stats_, WRITE_DONE_BY_SELF);
  auto write_done_by_other = write_group.size - 1;
  if (write_done_by_other > 0) {
    stats->AddDBStats(InternalStats::kIntStatsWriteDoneByOther,
                      write_done_by_other, concurrent_update);
    RecordTick(stats_, WRITE_DONE_BY_OTHER, write_done_by_other);
  }
  RecordInHistogram(stats_, BYTES_PER_WRITE, total_byte_size);

  PERF_TIMER_STOP(write_pre_and_post_process_time);

  PERF_TIMER_GUARD(write_wal_time);
  // LastAllocatedSequence is increased inside WriteToWAL under
  // wal_write_mutex_ to ensure ordered events in WAL
  size_t seq_inc = 0 /* total_count */;
  if (assign_order == kDoAssignOrder) {
    size_t total_batch_cnt = 0;
    for (auto* writer : write_group) {
      assert(writer->batch_cnt || !seq_per_batch_);
      if (!writer->CallbackFailed()) {
        total_batch_cnt += writer->batch_cnt;
      }
    }
    seq_inc = total_batch_cnt;
  }
  Status status;
  IOStatus io_s;
  io_s.PermitUncheckedError();  // Allow io_s to be uninitialized
  if (!write_options.disableWAL) {
    io_s = ConcurrentWriteToWAL(write_group, log_used, &last_sequence, seq_inc);
    status = io_s;
  } else {
    // Otherwise we inc seq number to do solely the seq allocation
    last_sequence = versions_->FetchAddLastAllocatedSequence(seq_inc);
  }

  size_t memtable_write_cnt = 0;
  auto curr_seq = last_sequence + 1;
  for (auto* writer : write_group) {
    if (writer->CallbackFailed()) {
      continue;
    }
    writer->sequence = curr_seq;
    if (assign_order == kDoAssignOrder) {
      assert(writer->batch_cnt || !seq_per_batch_);
      curr_seq += writer->batch_cnt;
    }
    if (!writer->disable_memtable) {
      memtable_write_cnt++;
    }
    // else seq advances only by memtable writes
  }
  if (status.ok() && write_options.sync) {
    assert(!write_options.disableWAL);
    // Requesting sync with two_write_queues_ is expected to be very rare. We
    // hance provide a simple implementation that is not necessarily efficient.
    if (manual_wal_flush_) {
      status = FlushWAL(true);
    } else {
      status = SyncWAL();
    }
  }
  PERF_TIMER_START(write_pre_and_post_process_time);

  if (!w.CallbackFailed()) {
    if (!io_s.ok()) {
      IOStatusCheck(io_s);
    } else {
      WriteStatusCheck(status);
    }
  }
  if (status.ok()) {
    size_t index = 0;
    for (auto* writer : write_group) {
      if (!writer->CallbackFailed() && writer->pre_release_callback) {
        assert(writer->sequence != kMaxSequenceNumber);
        Status ws = writer->pre_release_callback->Callback(
            writer->sequence, disable_memtable, writer->log_used, index++,
            pre_release_callback_cnt);
        if (!ws.ok()) {
          status = ws;
          break;
        }
      }
    }
  }
  if (publish_last_seq == kDoPublishLastSeq) {
    versions_->SetLastSequence(last_sequence + seq_inc);
    // Currently we only use kDoPublishLastSeq in unordered_write
    assert(immutable_db_options_.unordered_write);
  }
  if (immutable_db_options_.unordered_write && status.ok()) {
    pending_memtable_writes_ += memtable_write_cnt;
  }
  write_thread->ExitAsBatchGroupLeader(write_group, status);
  if (status.ok()) {
    status = w.FinalStatus();
  }
  if (seq_used != nullptr) {
    *seq_used = w.sequence;
  }
  return status;
}

void DBImpl::WriteStatusCheckOnLocked(const Status& status) {
  // Is setting bg_error_ enough here?  This will at least stop
  // compaction and fail any further writes.
  // Caller must hold mutex_.
  assert(!status.IsIOFenced() || !error_handler_.GetBGError().ok());
  mutex_.AssertHeld();
  if (immutable_db_options_.paranoid_checks && !status.ok() &&
      !status.IsBusy() && !status.IsIncomplete()) {
    // Maybe change the return status to void?
    error_handler_.SetBGError(status, BackgroundErrorReason::kWriteCallback);
  }
}

void DBImpl::WriteStatusCheck(const Status& status) {
  // Is setting bg_error_ enough here?  This will at least stop
  // compaction and fail any further writes.
  assert(!status.IsIOFenced() || !error_handler_.GetBGError().ok());
  if (immutable_db_options_.paranoid_checks && !status.ok() &&
      !status.IsBusy() && !status.IsIncomplete()) {
    mutex_.Lock();
    // Maybe change the return status to void?
    error_handler_.SetBGError(status, BackgroundErrorReason::kWriteCallback);
    mutex_.Unlock();
  }
}

void DBImpl::IOStatusCheck(const IOStatus& io_status) {
  // Is setting bg_error_ enough here?  This will at least stop
  // compaction and fail any further writes.
  if ((immutable_db_options_.paranoid_checks && !io_status.ok() &&
       !io_status.IsBusy() && !io_status.IsIncomplete()) ||
      io_status.IsIOFenced()) {
    mutex_.Lock();
    // Maybe change the return status to void?
    error_handler_.SetBGError(io_status, BackgroundErrorReason::kWriteCallback);
    mutex_.Unlock();
  }
}

void DBImpl::MemTableInsertStatusCheck(const Status& status) {
  // A non-OK status here indicates that the state implied by the
  // WAL has diverged from the in-memory state.  This could be
  // because of a corrupt write_batch (very bad), or because the
  // client specified an invalid column family and didn't specify
  // ignore_missing_column_families.
  if (!status.ok()) {
    mutex_.Lock();
    assert(!error_handler_.IsBGWorkStopped());
    // Maybe change the return status to void?
    error_handler_.SetBGError(status, BackgroundErrorReason::kMemTable)
        .PermitUncheckedError();
    mutex_.Unlock();
  }
}

Status DBImpl::PreprocessWrite(const WriteOptions& write_options,
                               bool* need_log_sync,
                               WriteContext* write_context) {
  mutex_.AssertHeld();
  assert(write_context != nullptr && need_log_sync != nullptr);
  Status status;

  if (error_handler_.IsDBStopped()) {
    status = error_handler_.GetBGError();
  }

  PERF_TIMER_GUARD(write_scheduling_flushes_compactions_time);

  assert(!single_column_family_mode_ ||
         versions_->GetColumnFamilySet()->NumberOfColumnFamilies() == 1);
  if (UNLIKELY(status.ok() && !single_column_family_mode_ &&
               total_log_size_ > GetMaxTotalWalSize())) {
    WaitForPendingWrites();
    status = SwitchWAL(write_context);
  }

  if (UNLIKELY(status.ok() && write_buffer_manager_->ShouldFlush())) {
    // Before a new memtable is added in SwitchMemtable(),
    // write_buffer_manager_->ShouldFlush() will keep returning true. If another
    // thread is writing to another DB with the same write buffer, they may also
    // be flushed. We may end up with flushing much more DBs than needed. It's
    // suboptimal but still correct.
    WaitForPendingWrites();
    status = HandleWriteBufferManagerFlush(write_context);
  }

  if (UNLIKELY(status.ok() && !trim_history_scheduler_.Empty())) {
    status = TrimMemtableHistory(write_context);
  }

  if (UNLIKELY(status.ok() && !flush_scheduler_.Empty())) {
    WaitForPendingWrites();
    status = ScheduleFlushes(write_context);
  }

  PERF_TIMER_STOP(write_scheduling_flushes_compactions_time);
  PERF_TIMER_GUARD(write_pre_and_post_process_time);

  if (UNLIKELY(status.ok() && (write_controller_.IsStopped() ||
                               write_controller_.NeedsDelay()))) {
    PERF_TIMER_STOP(write_pre_and_post_process_time);
    PERF_TIMER_GUARD(write_delay_time);
    // We don't know size of curent batch so that we always use the size
    // for previous one. It might create a fairness issue that expiration
    // might happen for smaller writes but larger writes can go through.
    // Can optimize it if it is an issue.
    status = DelayWrite(last_batch_group_size_, write_options);
    PERF_TIMER_START(write_pre_and_post_process_time);
  }

  // If memory usage exceeded beyond a certain threshold,
  // write_buffer_manager_->ShouldStall() returns true to all threads writing to
  // all DBs and writers will be stalled.
  // It does soft checking because WriteBufferManager::buffer_limit_ has already
  // exceeded at this point so no new write (including current one) will go
  // through until memory usage is decreased.
  if (UNLIKELY(status.ok() && write_buffer_manager_->ShouldStall())) {
    if (write_options.no_slowdown) {
      status = Status::Incomplete("Write stall");
    } else {
      WriteBufferManagerStallWrites();
    }
  }

  if (status.ok() && *need_log_sync) {
    // Wait until the parallel syncs are finished. Any sync process has to sync
    // the front log too so it is enough to check the status of front()
    // We do a while loop since log_sync_cv_ is signalled when any sync is
    // finished
    // Note: there does not seem to be a reason to wait for parallel sync at
    // this early step but it is not important since parallel sync (SyncWAL) and
    // need_log_sync are usually not used together.
    while (logs_.front().getting_synced) {
      log_sync_cv_.Wait();
    }
    for (auto& log : logs_) {
      assert(!log.getting_synced);
      // This is just to prevent the logs to be synced by a parallel SyncWAL
      // call. We will do the actual syncing later after we will write to the
      // WAL.
      // Note: there does not seem to be a reason to set this early before we
      // actually write to the WAL
      log.getting_synced = true;
    }
  } else {
    *need_log_sync = false;
  }

  return status;
}

WriteBatch* DBImpl::MergeBatch(const WriteThread::WriteGroup& write_group,
                               WriteBatch* tmp_batch, size_t* write_with_wal,
                               WriteBatch** to_be_cached_state) {
  assert(write_with_wal != nullptr);
  assert(tmp_batch != nullptr);
  assert(*to_be_cached_state == nullptr);
  WriteBatch* merged_batch = nullptr;
  *write_with_wal = 0;
  auto* leader = write_group.leader;
  assert(!leader->disable_wal);  // Same holds for all in the batch group
  if (write_group.size == 1 && !leader->CallbackFailed() &&
      leader->batch->GetWalTerminationPoint().is_cleared()) {
    // we simply write the first WriteBatch to WAL if the group only
    // contains one batch, that batch should be written to the WAL,
    // and the batch is not wanting to be truncated
    merged_batch = leader->batch;
    if (WriteBatchInternal::IsLatestPersistentState(merged_batch)) {
      *to_be_cached_state = merged_batch;
    }
    *write_with_wal = 1;
  } else {
    // WAL needs all of the batches flattened into a single batch.
    // We could avoid copying here with an iov-like AddRecord
    // interface
    merged_batch = tmp_batch;
    for (auto writer : write_group) {
      if (!writer->CallbackFailed()) {
        Status s = WriteBatchInternal::Append(merged_batch, writer->batch,
                                              /*WAL_only*/ true);
        // Always returns Status::OK.
        assert(s.ok());
        if (WriteBatchInternal::IsLatestPersistentState(writer->batch)) {
          // We only need to cache the last of such write batch
          *to_be_cached_state = writer->batch;
        }
        (*write_with_wal)++;
      }
    }
  }
  return merged_batch;
}

// When two_write_queues_ is disabled, this function is called from the only
// write thread. Otherwise this must be called holding log_write_mutex_.
IOStatus DBImpl::WriteToWAL(const WriteBatch& merged_batch,
                            log::Writer* log_writer, uint64_t* log_used,
                            uint64_t* log_size) {
  assert(log_size != nullptr);
  Slice log_entry = WriteBatchInternal::Contents(&merged_batch);
  *log_size = log_entry.size();
  // When two_write_queues_ WriteToWAL has to be protected from concurretn calls
  // from the two queues anyway and log_write_mutex_ is already held. Otherwise
  // if manual_wal_flush_ is enabled we need to protect log_writer->AddRecord
  // from possible concurrent calls via the FlushWAL by the application.
  const bool needs_locking = manual_wal_flush_ && !two_write_queues_;
  // Due to performance cocerns of missed branch prediction penalize the new
  // manual_wal_flush_ feature (by UNLIKELY) instead of the more common case
  // when we do not need any locking.
  if (UNLIKELY(needs_locking)) {
    log_write_mutex_.Lock();
  }
  IOStatus io_s = log_writer->AddRecord(log_entry);

  if (UNLIKELY(needs_locking)) {
    log_write_mutex_.Unlock();
  }
  if (log_used != nullptr) {
    *log_used = logfile_number_;
  }
  total_log_size_ += log_entry.size();
  // TODO(myabandeh): it might be unsafe to access alive_log_files_.back() here
  // since alive_log_files_ might be modified concurrently
  alive_log_files_.back().AddSize(log_entry.size());
  log_empty_ = false;
  return io_s;
}

IOStatus DBImpl::WriteToWAL(const WriteThread::WriteGroup& write_group,
                            log::Writer* log_writer, uint64_t* log_used,
                            bool need_log_sync, bool need_log_dir_sync,
                            SequenceNumber sequence) {
  IOStatus io_s;
  assert(!write_group.leader->disable_wal);
  // Same holds for all in the batch group
  size_t write_with_wal = 0;
  WriteBatch* to_be_cached_state = nullptr;
  WriteBatch* merged_batch = MergeBatch(write_group, &tmp_batch_,
                                        &write_with_wal, &to_be_cached_state);
  if (merged_batch == write_group.leader->batch) {
    write_group.leader->log_used = logfile_number_;
  } else if (write_with_wal > 1) {
    for (auto writer : write_group) {
      writer->log_used = logfile_number_;
    }
  }

  WriteBatchInternal::SetSequence(merged_batch, sequence);

  uint64_t log_size;
  io_s = WriteToWAL(*merged_batch, log_writer, log_used, &log_size);
  if (to_be_cached_state) {
    cached_recoverable_state_ = *to_be_cached_state;
    cached_recoverable_state_empty_ = false;
  }

  if (io_s.ok() && need_log_sync) {
    StopWatch sw(immutable_db_options_.clock, stats_, WAL_FILE_SYNC_MICROS);
    // It's safe to access logs_ with unlocked mutex_ here because:
    //  - we've set getting_synced=true for all logs,
    //    so other threads won't pop from logs_ while we're here,
    //  - only writer thread can push to logs_, and we're in
    //    writer thread, so no one will push to logs_,
    //  - as long as other threads don't modify it, it's safe to read
    //    from std::deque from multiple threads concurrently.
    //
    // Sync operation should work with locked log_write_mutex_, because:
    //   when DBOptions.manual_wal_flush_ is set,
    //   FlushWAL function will be invoked by another thread.
    //   if without locked log_write_mutex_, the log file may get data
    //   corruption

    const bool needs_locking = manual_wal_flush_ && !two_write_queues_;
    if (UNLIKELY(needs_locking)) {
      log_write_mutex_.Lock();
    }

    for (auto& log : logs_) {
      io_s = log.writer->file()->Sync(immutable_db_options_.use_fsync);
      if (!io_s.ok()) {
        break;
      }
    }

    if (UNLIKELY(needs_locking)) {
      log_write_mutex_.Unlock();
    }

    if (io_s.ok() && need_log_dir_sync) {
      // We only sync WAL directory the first time WAL syncing is
      // requested, so that in case users never turn on WAL sync,
      // we can avoid the disk I/O in the write code path.
      io_s = directories_.GetWalDir()->Fsync(IOOptions(), nullptr);
    }
  }

  if (merged_batch == &tmp_batch_) {
    tmp_batch_.Clear();
  }
  if (io_s.ok()) {
    auto stats = default_cf_internal_stats_;
    if (need_log_sync) {
      stats->AddDBStats(InternalStats::kIntStatsWalFileSynced, 1);
      RecordTick(stats_, WAL_FILE_SYNCED);
    }
    stats->AddDBStats(InternalStats::kIntStatsWalFileBytes, log_size);
    RecordTick(stats_, WAL_FILE_BYTES, log_size);
    stats->AddDBStats(InternalStats::kIntStatsWriteWithWal, write_with_wal);
    RecordTick(stats_, WRITE_WITH_WAL, write_with_wal);
  }
  return io_s;
}

IOStatus DBImpl::ConcurrentWriteToWAL(
    const WriteThread::WriteGroup& write_group, uint64_t* log_used,
    SequenceNumber* last_sequence, size_t seq_inc) {
  IOStatus io_s;

  assert(!write_group.leader->disable_wal);
  // Same holds for all in the batch group
  WriteBatch tmp_batch;
  size_t write_with_wal = 0;
  WriteBatch* to_be_cached_state = nullptr;
  WriteBatch* merged_batch =
      MergeBatch(write_group, &tmp_batch, &write_with_wal, &to_be_cached_state);

  // We need to lock log_write_mutex_ since logs_ and alive_log_files might be
  // pushed back concurrently
  log_write_mutex_.Lock();
  if (merged_batch == write_group.leader->batch) {
    write_group.leader->log_used = logfile_number_;
  } else if (write_with_wal > 1) {
    for (auto writer : write_group) {
      writer->log_used = logfile_number_;
    }
  }
  *last_sequence = versions_->FetchAddLastAllocatedSequence(seq_inc);
  auto sequence = *last_sequence + 1;
  WriteBatchInternal::SetSequence(merged_batch, sequence);

  log::Writer* log_writer = logs_.back().writer;
  uint64_t log_size;
  io_s = WriteToWAL(*merged_batch, log_writer, log_used, &log_size);
  if (to_be_cached_state) {
    cached_recoverable_state_ = *to_be_cached_state;
    cached_recoverable_state_empty_ = false;
  }
  log_write_mutex_.Unlock();

  if (io_s.ok()) {
    const bool concurrent = true;
    auto stats = default_cf_internal_stats_;
    stats->AddDBStats(InternalStats::kIntStatsWalFileBytes, log_size,
                      concurrent);
    RecordTick(stats_, WAL_FILE_BYTES, log_size);
    stats->AddDBStats(InternalStats::kIntStatsWriteWithWal, write_with_wal,
                      concurrent);
    RecordTick(stats_, WRITE_WITH_WAL, write_with_wal);
  }
  return io_s;
}

Status DBImpl::WriteRecoverableState() {
  mutex_.AssertHeld();
  if (!cached_recoverable_state_empty_) {
    bool dont_care_bool;
    SequenceNumber next_seq;
    if (two_write_queues_) {
      log_write_mutex_.Lock();
    }
    SequenceNumber seq;
    if (two_write_queues_) {
      seq = versions_->FetchAddLastAllocatedSequence(0);
    } else {
      seq = versions_->LastSequence();
    }
    WriteBatchInternal::SetSequence(&cached_recoverable_state_, seq + 1);
    auto status = WriteBatchInternal::InsertInto(
        &cached_recoverable_state_, column_family_memtables_.get(),
        &flush_scheduler_, &trim_history_scheduler_, true,
        0 /*recovery_log_number*/, this, false /* concurrent_memtable_writes */,
        &next_seq, &dont_care_bool, seq_per_batch_);
    auto last_seq = next_seq - 1;
    if (two_write_queues_) {
      versions_->FetchAddLastAllocatedSequence(last_seq - seq);
      versions_->SetLastPublishedSequence(last_seq);
    }
    versions_->SetLastSequence(last_seq);
    if (two_write_queues_) {
      log_write_mutex_.Unlock();
    }
    if (status.ok() && recoverable_state_pre_release_callback_) {
      const bool DISABLE_MEMTABLE = true;
      for (uint64_t sub_batch_seq = seq + 1;
           sub_batch_seq < next_seq && status.ok(); sub_batch_seq++) {
        uint64_t const no_log_num = 0;
        // Unlock it since the callback might end up locking mutex. e.g.,
        // AddCommitted -> AdvanceMaxEvictedSeq -> GetSnapshotListFromDB
        mutex_.Unlock();
        status = recoverable_state_pre_release_callback_->Callback(
            sub_batch_seq, !DISABLE_MEMTABLE, no_log_num, 0, 1);
        mutex_.Lock();
      }
    }
    if (status.ok()) {
      cached_recoverable_state_.Clear();
      cached_recoverable_state_empty_ = true;
    }
    return status;
  }
  return Status::OK();
}

void DBImpl::SelectColumnFamiliesForAtomicFlush(
    autovector<ColumnFamilyData*>* cfds) {
  for (ColumnFamilyData* cfd : *versions_->GetColumnFamilySet()) {
    if (cfd->IsDropped()) {
      continue;
    }
    if (cfd->imm()->NumNotFlushed() != 0 || !cfd->mem()->IsEmpty() ||
        !cached_recoverable_state_empty_.load()) {
      cfds->push_back(cfd);
    }
  }
}

// Assign sequence number for atomic flush.
void DBImpl::AssignAtomicFlushSeq(const autovector<ColumnFamilyData*>& cfds) {
  assert(immutable_db_options_.atomic_flush);
  auto seq = versions_->LastSequence();
  for (auto cfd : cfds) {
    cfd->imm()->AssignAtomicFlushSeq(seq);
  }
}

Status DBImpl::SwitchWAL(WriteContext* write_context) {
  mutex_.AssertHeld();
  assert(write_context != nullptr);
  Status status;

  if (alive_log_files_.begin()->getting_flushed) {
    return status;
  }

  auto oldest_alive_log = alive_log_files_.begin()->number;
  bool flush_wont_release_oldest_log = false;
  if (allow_2pc()) {
    auto oldest_log_with_uncommitted_prep =
        logs_with_prep_tracker_.FindMinLogContainingOutstandingPrep();

    assert(oldest_log_with_uncommitted_prep == 0 ||
           oldest_log_with_uncommitted_prep >= oldest_alive_log);
    if (oldest_log_with_uncommitted_prep > 0 &&
        oldest_log_with_uncommitted_prep == oldest_alive_log) {
      if (unable_to_release_oldest_log_) {
        // we already attempted to flush all column families dependent on
        // the oldest alive log but the log still contained uncommitted
        // transactions so there is still nothing that we can do.
        return status;
      } else {
        ROCKS_LOG_WARN(
            immutable_db_options_.info_log,
            "Unable to release oldest log due to uncommitted transaction");
        unable_to_release_oldest_log_ = true;
        flush_wont_release_oldest_log = true;
      }
    }
  }
  if (!flush_wont_release_oldest_log) {
    // we only mark this log as getting flushed if we have successfully
    // flushed all data in this log. If this log contains outstanding prepared
    // transactions then we cannot flush this log until those transactions are
    // commited.
    unable_to_release_oldest_log_ = false;
    alive_log_files_.begin()->getting_flushed = true;
  }

  ROCKS_LOG_INFO(
      immutable_db_options_.info_log,
      "Flushing all column families with data in WAL number %" PRIu64
      ". Total log size is %" PRIu64 " while max_total_wal_size is %" PRIu64,
      oldest_alive_log, total_log_size_.load(), GetMaxTotalWalSize());
  // no need to refcount because drop is happening in write thread, so can't
  // happen while we're in the write thread
  autovector<ColumnFamilyData*> cfds;
  if (immutable_db_options_.atomic_flush) {
    SelectColumnFamiliesForAtomicFlush(&cfds);
  } else {
    for (auto cfd : *versions_->GetColumnFamilySet()) {
      if (cfd->IsDropped()) {
        continue;
      }
      if (cfd->OldestLogToKeep() <= oldest_alive_log) {
        cfds.push_back(cfd);
      }
    }
    MaybeFlushStatsCF(&cfds);
  }
  WriteThread::Writer nonmem_w;
  if (two_write_queues_) {
    nonmem_write_thread_.EnterUnbatched(&nonmem_w, &mutex_);
  }

  for (const auto cfd : cfds) {
    cfd->Ref();
    status = SwitchMemtable(cfd, write_context);
    cfd->UnrefAndTryDelete();
    if (!status.ok()) {
      break;
    }
  }
  if (two_write_queues_) {
    nonmem_write_thread_.ExitUnbatched(&nonmem_w);
  }

  if (status.ok()) {
    if (immutable_db_options_.atomic_flush) {
      AssignAtomicFlushSeq(cfds);
    }
    for (auto cfd : cfds) {
      cfd->imm()->FlushRequested();
      if (!immutable_db_options_.atomic_flush) {
        FlushRequest flush_req;
        GenerateFlushRequest({cfd}, &flush_req);
        SchedulePendingFlush(flush_req, FlushReason::kWalFull);
      }
    }
    if (immutable_db_options_.atomic_flush) {
      FlushRequest flush_req;
      GenerateFlushRequest(cfds, &flush_req);
      SchedulePendingFlush(flush_req, FlushReason::kWalFull);
    }
    MaybeScheduleFlushOrCompaction();
  }
  return status;
}

Status DBImpl::HandleWriteBufferManagerFlush(WriteContext* write_context) {
  mutex_.AssertHeld();
  assert(write_context != nullptr);
  Status status;

  // Before a new memtable is added in SwitchMemtable(),
  // write_buffer_manager_->ShouldFlush() will keep returning true. If another
  // thread is writing to another DB with the same write buffer, they may also
  // be flushed. We may end up with flushing much more DBs than needed. It's
  // suboptimal but still correct.
  ROCKS_LOG_INFO(
      immutable_db_options_.info_log,
      "Flushing column family with oldest memtable entry. Write buffers are "
      "using %" ROCKSDB_PRIszt " bytes out of a total of %" ROCKSDB_PRIszt ".",
      write_buffer_manager_->memory_usage(),
      write_buffer_manager_->buffer_size());
  // no need to refcount because drop is happening in write thread, so can't
  // happen while we're in the write thread
  autovector<ColumnFamilyData*> cfds;
  if (immutable_db_options_.atomic_flush) {
    SelectColumnFamiliesForAtomicFlush(&cfds);
  } else {
    ColumnFamilyData* cfd_picked = nullptr;
    SequenceNumber seq_num_for_cf_picked = kMaxSequenceNumber;

    for (auto cfd : *versions_->GetColumnFamilySet()) {
      if (cfd->IsDropped()) {
        continue;
      }
      if (!cfd->mem()->IsEmpty()) {
        // We only consider active mem table, hoping immutable memtable is
        // already in the process of flushing.
        uint64_t seq = cfd->mem()->GetCreationSeq();
        if (cfd_picked == nullptr || seq < seq_num_for_cf_picked) {
          cfd_picked = cfd;
          seq_num_for_cf_picked = seq;
        }
      }
    }
    if (cfd_picked != nullptr) {
      cfds.push_back(cfd_picked);
    }
    MaybeFlushStatsCF(&cfds);
  }

  WriteThread::Writer nonmem_w;
  if (two_write_queues_) {
    nonmem_write_thread_.EnterUnbatched(&nonmem_w, &mutex_);
  }
  for (const auto cfd : cfds) {
    if (cfd->mem()->IsEmpty()) {
      continue;
    }
    cfd->Ref();
    status = SwitchMemtable(cfd, write_context);
    cfd->UnrefAndTryDelete();
    if (!status.ok()) {
      break;
    }
  }
  if (two_write_queues_) {
    nonmem_write_thread_.ExitUnbatched(&nonmem_w);
  }

  if (status.ok()) {
    if (immutable_db_options_.atomic_flush) {
      AssignAtomicFlushSeq(cfds);
    }
    for (const auto cfd : cfds) {
      cfd->imm()->FlushRequested();
      if (!immutable_db_options_.atomic_flush) {
        FlushRequest flush_req;
        GenerateFlushRequest({cfd}, &flush_req);
        SchedulePendingFlush(flush_req, FlushReason::kWriteBufferManager);
      }
    }
    if (immutable_db_options_.atomic_flush) {
      FlushRequest flush_req;
      GenerateFlushRequest(cfds, &flush_req);
      SchedulePendingFlush(flush_req, FlushReason::kWriteBufferManager);
    }
    MaybeScheduleFlushOrCompaction();
  }
  return status;
}

uint64_t DBImpl::GetMaxTotalWalSize() const {
  mutex_.AssertHeld();
  return mutable_db_options_.max_total_wal_size == 0
             ? 4 * max_total_in_memory_state_
             : mutable_db_options_.max_total_wal_size;
}

// REQUIRES: mutex_ is held
// REQUIRES: this thread is currently at the front of the writer queue
Status DBImpl::DelayWrite(uint64_t num_bytes,
                          const WriteOptions& write_options) {
  uint64_t time_delayed = 0;
  bool delayed = false;
  {
    StopWatch sw(immutable_db_options_.clock, stats_, WRITE_STALL,
                 &time_delayed);
    uint64_t delay =
        write_controller_.GetDelay(immutable_db_options_.clock, num_bytes);
    if (delay > 0) {
      if (write_options.no_slowdown) {
        return Status::Incomplete("Write stall");
      }
      TEST_SYNC_POINT("DBImpl::DelayWrite:Sleep");

      // Notify write_thread_ about the stall so it can setup a barrier and
      // fail any pending writers with no_slowdown
      write_thread_.BeginWriteStall();
      TEST_SYNC_POINT("DBImpl::DelayWrite:BeginWriteStallDone");
      mutex_.Unlock();
      // We will delay the write until we have slept for `delay` microseconds
      // or we don't need a delay anymore. We check for cancellation every 1ms
      // (slightly longer because WriteController minimum delay is 1ms, in
      // case of sleep imprecision, rounding, etc.)
      const uint64_t kDelayInterval = 1001;
      uint64_t stall_end = sw.start_time() + delay;
      while (write_controller_.NeedsDelay()) {
        if (immutable_db_options_.clock->NowMicros() >= stall_end) {
          // We already delayed this write `delay` microseconds
          break;
        }

        delayed = true;
        // Sleep for 0.001 seconds
        immutable_db_options_.clock->SleepForMicroseconds(kDelayInterval);
      }
      mutex_.Lock();
      write_thread_.EndWriteStall();
    }

    // Don't wait if there's a background error, even if its a soft error. We
    // might wait here indefinitely as the background compaction may never
    // finish successfully, resulting in the stall condition lasting
    // indefinitely
    while (error_handler_.GetBGError().ok() && write_controller_.IsStopped()) {
      if (write_options.no_slowdown) {
        return Status::Incomplete("Write stall");
      }
      delayed = true;

      // Notify write_thread_ about the stall so it can setup a barrier and
      // fail any pending writers with no_slowdown
      write_thread_.BeginWriteStall();
      TEST_SYNC_POINT("DBImpl::DelayWrite:Wait");
      bg_cv_.Wait();
      write_thread_.EndWriteStall();
    }
  }
  assert(!delayed || !write_options.no_slowdown);
  if (delayed) {
    default_cf_internal_stats_->AddDBStats(
        InternalStats::kIntStatsWriteStallMicros, time_delayed);
    RecordTick(stats_, STALL_MICROS, time_delayed);
  }

  // If DB is not in read-only mode and write_controller is not stopping
  // writes, we can ignore any background errors and allow the write to
  // proceed
  Status s;
  if (write_controller_.IsStopped()) {
    // If writes are still stopped, it means we bailed due to a background
    // error
    s = Status::Incomplete(error_handler_.GetBGError().ToString());
  }
  if (error_handler_.IsDBStopped()) {
    s = error_handler_.GetBGError();
  }
  return s;
}

// REQUIRES: mutex_ is held
// REQUIRES: this thread is currently at the front of the writer queue
void DBImpl::WriteBufferManagerStallWrites() {
  mutex_.AssertHeld();
  // First block future writer threads who want to add themselves to the queue
  // of WriteThread.
  write_thread_.BeginWriteStall();
  mutex_.Unlock();

  // Change the state to State::Blocked.
  static_cast<WBMStallInterface*>(wbm_stall_.get())
      ->SetState(WBMStallInterface::State::BLOCKED);
  // Then WriteBufferManager will add DB instance to its queue
  // and block this thread by calling WBMStallInterface::Block().
  write_buffer_manager_->BeginWriteStall(wbm_stall_.get());
  wbm_stall_->Block();

  mutex_.Lock();
  // Stall has ended. Signal writer threads so that they can add
  // themselves to the WriteThread queue for writes.
  write_thread_.EndWriteStall();
}

Status DBImpl::ThrottleLowPriWritesIfNeeded(const WriteOptions& write_options,
                                            WriteBatch* my_batch) {
  assert(write_options.low_pri);
  // This is called outside the DB mutex. Although it is safe to make the call,
  // the consistency condition is not guaranteed to hold. It's OK to live with
  // it in this case.
  // If we need to speed compaction, it means the compaction is left behind
  // and we start to limit low pri writes to a limit.
  if (write_controller_.NeedSpeedupCompaction()) {
    if (allow_2pc() && (my_batch->HasCommit() || my_batch->HasRollback())) {
      // For 2PC, we only rate limit prepare, not commit.
      return Status::OK();
    }
    if (write_options.no_slowdown) {
      return Status::Incomplete("Low priority write stall");
    } else {
      assert(my_batch != nullptr);
      // Rate limit those writes. The reason that we don't completely wait
      // is that in case the write is heavy, low pri writes may never have
      // a chance to run. Now we guarantee we are still slowly making
      // progress.
      PERF_TIMER_GUARD(write_delay_time);
      write_controller_.low_pri_rate_limiter()->Request(
          my_batch->GetDataSize(), Env::IO_HIGH, nullptr /* stats */,
          RateLimiter::OpType::kWrite);
    }
  }
  return Status::OK();
}

void DBImpl::MaybeFlushStatsCF(autovector<ColumnFamilyData*>* cfds) {
  assert(cfds != nullptr);
  if (!cfds->empty() && immutable_db_options_.persist_stats_to_disk) {
    ColumnFamilyData* cfd_stats =
        versions_->GetColumnFamilySet()->GetColumnFamily(
            kPersistentStatsColumnFamilyName);
    if (cfd_stats != nullptr && !cfd_stats->mem()->IsEmpty()) {
      for (ColumnFamilyData* cfd : *cfds) {
        if (cfd == cfd_stats) {
          // stats CF already included in cfds
          return;
        }
      }
      // force flush stats CF when its log number is less than all other CF's
      // log numbers
      bool force_flush_stats_cf = true;
      for (auto* loop_cfd : *versions_->GetColumnFamilySet()) {
        if (loop_cfd == cfd_stats) {
          continue;
        }
        if (loop_cfd->GetLogNumber() <= cfd_stats->GetLogNumber()) {
          force_flush_stats_cf = false;
        }
      }
      if (force_flush_stats_cf) {
        cfds->push_back(cfd_stats);
        ROCKS_LOG_INFO(immutable_db_options_.info_log,
                       "Force flushing stats CF with automated flush "
                       "to avoid holding old logs");
      }
    }
  }
}

Status DBImpl::TrimMemtableHistory(WriteContext* context) {
  autovector<ColumnFamilyData*> cfds;
  ColumnFamilyData* tmp_cfd;
  while ((tmp_cfd = trim_history_scheduler_.TakeNextColumnFamily()) !=
         nullptr) {
    cfds.push_back(tmp_cfd);
  }
  for (auto& cfd : cfds) {
    autovector<MemTable*> to_delete;
    bool trimmed = cfd->imm()->TrimHistory(
        &context->memtables_to_free_, cfd->mem()->ApproximateMemoryUsage());
    if (trimmed) {
      context->superversion_context.NewSuperVersion();
      assert(context->superversion_context.new_superversion.get() != nullptr);
      cfd->InstallSuperVersion(&context->superversion_context, &mutex_);
    }

    if (cfd->UnrefAndTryDelete()) {
      cfd = nullptr;
    }
  }
  return Status::OK();
}

Status DBImpl::ScheduleFlushes(WriteContext* context) {
  autovector<ColumnFamilyData*> cfds;
  if (immutable_db_options_.atomic_flush) {
    SelectColumnFamiliesForAtomicFlush(&cfds);
    for (auto cfd : cfds) {
      cfd->Ref();
    }
    flush_scheduler_.Clear();
  } else {
    ColumnFamilyData* tmp_cfd;
    while ((tmp_cfd = flush_scheduler_.TakeNextColumnFamily()) != nullptr) {
      cfds.push_back(tmp_cfd);
    }
    MaybeFlushStatsCF(&cfds);
  }
  Status status;
  WriteThread::Writer nonmem_w;
  if (two_write_queues_) {
    nonmem_write_thread_.EnterUnbatched(&nonmem_w, &mutex_);
  }

  for (auto& cfd : cfds) {
    if (!cfd->mem()->IsEmpty()) {
      status = SwitchMemtable(cfd, context);
    }
    if (cfd->UnrefAndTryDelete()) {
      cfd = nullptr;
    }
    if (!status.ok()) {
      break;
    }
  }

  if (two_write_queues_) {
    nonmem_write_thread_.ExitUnbatched(&nonmem_w);
  }

  if (status.ok()) {
    if (immutable_db_options_.atomic_flush) {
      AssignAtomicFlushSeq(cfds);
      FlushRequest flush_req;
      GenerateFlushRequest(cfds, &flush_req);
      SchedulePendingFlush(flush_req, FlushReason::kWriteBufferFull);
    } else {
      for (auto* cfd : cfds) {
        FlushRequest flush_req;
        GenerateFlushRequest({cfd}, &flush_req);
        SchedulePendingFlush(flush_req, FlushReason::kWriteBufferFull);
      }
    }
    MaybeScheduleFlushOrCompaction();
  }
  return status;
}

#ifndef ROCKSDB_LITE
void DBImpl::NotifyOnMemTableSealed(ColumnFamilyData* /*cfd*/,
                                    const MemTableInfo& mem_table_info) {
  if (immutable_db_options_.listeners.size() == 0U) {
    return;
  }
  if (shutting_down_.load(std::memory_order_acquire)) {
    return;
  }

  for (auto listener : immutable_db_options_.listeners) {
    listener->OnMemTableSealed(mem_table_info);
  }
}
#endif  // ROCKSDB_LITE

// REQUIRES: mutex_ is held
// REQUIRES: this thread is currently at the front of the writer queue
// REQUIRES: this thread is currently at the front of the 2nd writer queue if
// two_write_queues_ is true (This is to simplify the reasoning.)
Status DBImpl::SwitchMemtable(ColumnFamilyData* cfd, WriteContext* context) {
  mutex_.AssertHeld();
  log::Writer* new_log = nullptr;
  MemTable* new_mem = nullptr;
  IOStatus io_s;

  // Recoverable state is persisted in WAL. After memtable switch, WAL might
  // be deleted, so we write the state to memtable to be persisted as well.
  Status s = WriteRecoverableState();
  if (!s.ok()) {
    return s;
  }

  // Attempt to switch to a new memtable and trigger flush of old.
  // Do this without holding the dbmutex lock.
  assert(versions_->prev_log_number() == 0);
  if (two_write_queues_) {
    log_write_mutex_.Lock();
  }
  bool creating_new_log = !log_empty_;
  if (two_write_queues_) {
    log_write_mutex_.Unlock();
  }
  uint64_t recycle_log_number = 0;
  if (creating_new_log && immutable_db_options_.recycle_log_file_num &&
      !log_recycle_files_.empty()) {
    recycle_log_number = log_recycle_files_.front();
  }
  uint64_t new_log_number =
      creating_new_log ? versions_->NewFileNumber() : logfile_number_;
  const MutableCFOptions mutable_cf_options = *cfd->GetLatestMutableCFOptions();

  // Set memtable_info for memtable sealed callback
#ifndef ROCKSDB_LITE
  MemTableInfo memtable_info;
  memtable_info.cf_name = cfd->GetName();
  memtable_info.first_seqno = cfd->mem()->GetFirstSequenceNumber();
  memtable_info.earliest_seqno = cfd->mem()->GetEarliestSequenceNumber();
  memtable_info.num_entries = cfd->mem()->num_entries();
  memtable_info.num_deletes = cfd->mem()->num_deletes();
#endif  // ROCKSDB_LITE
  // Log this later after lock release. It may be outdated, e.g., if background
  // flush happens before logging, but that should be ok.
  int num_imm_unflushed = cfd->imm()->NumNotFlushed();
  const auto preallocate_block_size =
      GetWalPreallocateBlockSize(mutable_cf_options.write_buffer_size);
  mutex_.Unlock();
  if (creating_new_log) {
    // TODO: Write buffer size passed in should be max of all CF's instead
    // of mutable_cf_options.write_buffer_size.
    io_s = CreateWAL(new_log_number, recycle_log_number, preallocate_block_size,
                     &new_log);
    if (s.ok()) {
      s = io_s;
    }
  }
  if (s.ok()) {
    SequenceNumber seq = versions_->LastSequence();
    new_mem = cfd->ConstructNewMemtable(mutable_cf_options, seq);
    context->superversion_context.NewSuperVersion();
  }
  ROCKS_LOG_INFO(immutable_db_options_.info_log,
                 "[%s] New memtable created with log file: #%" PRIu64
                 ". Immutable memtables: %d.\n",
                 cfd->GetName().c_str(), new_log_number, num_imm_unflushed);
  mutex_.Lock();
  if (recycle_log_number != 0) {
    // Since renaming the file is done outside DB mutex, we need to ensure
    // concurrent full purges don't delete the file while we're recycling it.
    // To achieve that we hold the old log number in the recyclable list until
    // after it has been renamed.
    assert(log_recycle_files_.front() == recycle_log_number);
    log_recycle_files_.pop_front();
  }
  if (s.ok() && creating_new_log) {
    log_write_mutex_.Lock();
    assert(new_log != nullptr);
    if (!logs_.empty()) {
      // Alway flush the buffer of the last log before switching to a new one
      log::Writer* cur_log_writer = logs_.back().writer;
      io_s = cur_log_writer->WriteBuffer();
      if (s.ok()) {
        s = io_s;
      }
      if (!s.ok()) {
        ROCKS_LOG_WARN(immutable_db_options_.info_log,
                       "[%s] Failed to switch from #%" PRIu64 " to #%" PRIu64
                       "  WAL file\n",
                       cfd->GetName().c_str(), cur_log_writer->get_log_number(),
                       new_log_number);
      }
    }
    if (s.ok()) {
      logfile_number_ = new_log_number;
      log_empty_ = true;
      log_dir_synced_ = false;
      logs_.emplace_back(logfile_number_, new_log);
      alive_log_files_.push_back(LogFileNumberSize(logfile_number_));
    }
    log_write_mutex_.Unlock();
  }

  if (!s.ok()) {
    // how do we fail if we're not creating new log?
    assert(creating_new_log);
    delete new_mem;
    delete new_log;
    context->superversion_context.new_superversion.reset();
    // We may have lost data from the WritableFileBuffer in-memory buffer for
    // the current log, so treat it as a fatal error and set bg_error
    if (!io_s.ok()) {
      error_handler_.SetBGError(io_s, BackgroundErrorReason::kMemTable);
    } else {
      error_handler_.SetBGError(s, BackgroundErrorReason::kMemTable);
    }
    // Read back bg_error in order to get the right severity
    s = error_handler_.GetBGError();
    return s;
  }

  bool empty_cf_updated = false;
  if (immutable_db_options_.track_and_verify_wals_in_manifest &&
      !immutable_db_options_.allow_2pc && creating_new_log) {
    // In non-2pc mode, WALs become obsolete if they do not contain unflushed
    // data. Updating the empty CF's log number might cause some WALs to become
    // obsolete. So we should track the WAL obsoletion event before actually
    // updating the empty CF's log number.
    uint64_t min_wal_number_to_keep =
        versions_->PreComputeMinLogNumberWithUnflushedData(logfile_number_);
    if (min_wal_number_to_keep >
        versions_->GetWalSet().GetMinWalNumberToKeep()) {
      // Get a snapshot of the empty column families.
      // LogAndApply may release and reacquire db
      // mutex, during that period, column family may become empty (e.g. its
      // flush succeeds), then it affects the computed min_log_number_to_keep,
      // so we take a snapshot for consistency of column family data
      // status. If a column family becomes non-empty afterwards, its active log
      // should still be the created new log, so the min_log_number_to_keep is
      // not affected.
      autovector<ColumnFamilyData*> empty_cfs;
      for (auto cf : *versions_->GetColumnFamilySet()) {
        if (cf->IsEmpty()) {
          empty_cfs.push_back(cf);
        }
      }

      VersionEdit wal_deletion;
      wal_deletion.DeleteWalsBefore(min_wal_number_to_keep);
      s = versions_->LogAndApplyToDefaultColumnFamily(&wal_deletion, &mutex_);
      if (!s.ok() && versions_->io_status().IsIOError()) {
        s = error_handler_.SetBGError(versions_->io_status(),
                                      BackgroundErrorReason::kManifestWrite);
      }
      if (!s.ok()) {
        return s;
      }

      for (auto cf : empty_cfs) {
        if (cf->IsEmpty()) {
          cf->SetLogNumber(logfile_number_);
          // MEMPURGE: No need to change this, because new adds
          // should still receive new sequence numbers.
          cf->mem()->SetCreationSeq(versions_->LastSequence());
        }  // cf may become non-empty.
      }
      empty_cf_updated = true;
    }
  }
  if (!empty_cf_updated) {
    for (auto cf : *versions_->GetColumnFamilySet()) {
      // all this is just optimization to delete logs that
      // are no longer needed -- if CF is empty, that means it
      // doesn't need that particular log to stay alive, so we just
      // advance the log number. no need to persist this in the manifest
      if (cf->IsEmpty()) {
        if (creating_new_log) {
          cf->SetLogNumber(logfile_number_);
        }
        cf->mem()->SetCreationSeq(versions_->LastSequence());
      }
    }
  }

  cfd->mem()->SetNextLogNumber(logfile_number_);
  assert(new_mem != nullptr);
  cfd->imm()->Add(cfd->mem(), &context->memtables_to_free_);
  new_mem->Ref();
  cfd->SetMemtable(new_mem);
  InstallSuperVersionAndScheduleWork(cfd, &context->superversion_context,
                                     mutable_cf_options);

#ifndef ROCKSDB_LITE
  mutex_.Unlock();
  // Notify client that memtable is sealed, now that we have successfully
  // installed a new memtable
  NotifyOnMemTableSealed(cfd, memtable_info);
  mutex_.Lock();
#endif  // ROCKSDB_LITE
  // It is possible that we got here without checking the value of i_os, but
  // that is okay.  If we did, it most likely means that s was already an error.
  // In any case, ignore any unchecked error for i_os here.
  io_s.PermitUncheckedError();
  return s;
}

size_t DBImpl::GetWalPreallocateBlockSize(uint64_t write_buffer_size) const {
  mutex_.AssertHeld();
  size_t bsize =
      static_cast<size_t>(write_buffer_size / 10 + write_buffer_size);
  // Some users might set very high write_buffer_size and rely on
  // max_total_wal_size or other parameters to control the WAL size.
  if (mutable_db_options_.max_total_wal_size > 0) {
    bsize = std::min<size_t>(
        bsize, static_cast<size_t>(mutable_db_options_.max_total_wal_size));
  }
  if (immutable_db_options_.db_write_buffer_size > 0) {
    bsize = std::min<size_t>(bsize, immutable_db_options_.db_write_buffer_size);
  }
  if (immutable_db_options_.write_buffer_manager &&
      immutable_db_options_.write_buffer_manager->enabled()) {
    bsize = std::min<size_t>(
        bsize, immutable_db_options_.write_buffer_manager->buffer_size());
  }

  return bsize;
}

// Default implementations of convenience methods that subclasses of DB
// can call if they wish
Status DB::Put(const WriteOptions& opt, ColumnFamilyHandle* column_family,
               const Slice& key, const Slice& value) {
  if (nullptr == opt.timestamp) {
    // Pre-allocate size of write batch conservatively.
    // 8 bytes are taken by header, 4 bytes for count, 1 byte for type,
    // and we allocate 11 extra bytes for key length, as well as value length.
    WriteBatch batch(key.size() + value.size() + 24);
    Status s = batch.Put(column_family, key, value);
    if (!s.ok()) {
      return s;
    }
    return Write(opt, &batch);
  }
  const Slice* ts = opt.timestamp;
  assert(nullptr != ts);
  size_t ts_sz = ts->size();
  assert(column_family->GetComparator());
  assert(ts_sz == column_family->GetComparator()->timestamp_size());
  WriteBatch batch;
  Status s;
  if (key.data() + key.size() == ts->data()) {
    Slice key_with_ts = Slice(key.data(), key.size() + ts_sz);
    s = batch.Put(column_family, key_with_ts, value);
  } else {
    std::array<Slice, 2> key_with_ts_slices{{key, *ts}};
    SliceParts key_with_ts(key_with_ts_slices.data(), 2);
    std::array<Slice, 1> value_slices{{value}};
    SliceParts values(value_slices.data(), 1);
    s = batch.Put(column_family, key_with_ts, values);
  }
  if (!s.ok()) {
    return s;
  }
  return Write(opt, &batch);
}

Status DB::Delete(const WriteOptions& opt, ColumnFamilyHandle* column_family,
                  const Slice& key) {
  if (nullptr == opt.timestamp) {
    WriteBatch batch;
    Status s = batch.Delete(column_family, key);
    if (!s.ok()) {
      return s;
    }
    return Write(opt, &batch);
  }
  const Slice* ts = opt.timestamp;
  assert(ts != nullptr);
  size_t ts_sz = ts->size();
  assert(column_family->GetComparator());
  assert(ts_sz == column_family->GetComparator()->timestamp_size());
  WriteBatch batch;
  Status s;
  if (key.data() + key.size() == ts->data()) {
    Slice key_with_ts = Slice(key.data(), key.size() + ts_sz);
    s = batch.Delete(column_family, key_with_ts);
  } else {
    std::array<Slice, 2> key_with_ts_slices{{key, *ts}};
    SliceParts key_with_ts(key_with_ts_slices.data(), 2);
    s = batch.Delete(column_family, key_with_ts);
  }
  if (!s.ok()) {
    return s;
  }
  return Write(opt, &batch);
}

Status DB::SingleDelete(const WriteOptions& opt,
                        ColumnFamilyHandle* column_family, const Slice& key) {
  Status s;
  if (opt.timestamp == nullptr) {
    WriteBatch batch;
    s = batch.SingleDelete(column_family, key);
    if (!s.ok()) {
      return s;
    }
    s = Write(opt, &batch);
    return s;
  }

  const Slice* ts = opt.timestamp;
  assert(ts != nullptr);
  size_t ts_sz = ts->size();
  assert(column_family->GetComparator());
  assert(ts_sz == column_family->GetComparator()->timestamp_size());
  WriteBatch batch;
  if (key.data() + key.size() == ts->data()) {
    Slice key_with_ts = Slice(key.data(), key.size() + ts_sz);
    s = batch.SingleDelete(column_family, key_with_ts);
  } else {
    std::array<Slice, 2> key_with_ts_slices{{key, *ts}};
    SliceParts key_with_ts(key_with_ts_slices.data(), 2);
    s = batch.SingleDelete(column_family, key_with_ts);
  }
  if (!s.ok()) {
    return s;
  }
  s = Write(opt, &batch);
  return s;
}

Status DB::DeleteRange(const WriteOptions& opt,
                       ColumnFamilyHandle* column_family,
                       const Slice& begin_key, const Slice& end_key) {
  WriteBatch batch;
  Status s = batch.DeleteRange(column_family, begin_key, end_key);
  if (!s.ok()) {
    return s;
  }
  return Write(opt, &batch);
}

Status DB::Merge(const WriteOptions& opt, ColumnFamilyHandle* column_family,
                 const Slice& key, const Slice& value) {
  WriteBatch batch;
  Status s = batch.Merge(column_family, key, value);
  if (!s.ok()) {
    return s;
  }
  return Write(opt, &batch);
}
}  // namespace ROCKSDB_NAMESPACE
