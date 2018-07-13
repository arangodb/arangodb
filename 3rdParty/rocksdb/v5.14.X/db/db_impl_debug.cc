//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef NDEBUG

#include "db/db_impl.h"
#include "monitoring/thread_status_updater.h"

namespace rocksdb {

uint64_t DBImpl::TEST_GetLevel0TotalSize() {
  InstrumentedMutexLock l(&mutex_);
  return default_cf_handle_->cfd()->current()->storage_info()->NumLevelBytes(0);
}

void DBImpl::TEST_SwitchWAL() {
  WriteContext write_context;
  InstrumentedMutexLock l(&mutex_);
  SwitchWAL(&write_context);
}

bool DBImpl::TEST_WALBufferIsEmpty() {
  InstrumentedMutexLock wl(&log_write_mutex_);
  log::Writer* cur_log_writer = logs_.back().writer;
  return cur_log_writer->TEST_BufferIsEmpty();
}

int64_t DBImpl::TEST_MaxNextLevelOverlappingBytes(
    ColumnFamilyHandle* column_family) {
  ColumnFamilyData* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<ColumnFamilyHandleImpl*>(column_family);
    cfd = cfh->cfd();
  }
  InstrumentedMutexLock l(&mutex_);
  return cfd->current()->storage_info()->MaxNextLevelOverlappingBytes();
}

void DBImpl::TEST_GetFilesMetaData(
    ColumnFamilyHandle* column_family,
    std::vector<std::vector<FileMetaData>>* metadata) {
  auto cfh = reinterpret_cast<ColumnFamilyHandleImpl*>(column_family);
  auto cfd = cfh->cfd();
  InstrumentedMutexLock l(&mutex_);
  metadata->resize(NumberLevels());
  for (int level = 0; level < NumberLevels(); level++) {
    const std::vector<FileMetaData*>& files =
        cfd->current()->storage_info()->LevelFiles(level);

    (*metadata)[level].clear();
    for (const auto& f : files) {
      (*metadata)[level].push_back(*f);
    }
  }
}

uint64_t DBImpl::TEST_Current_Manifest_FileNo() {
  return versions_->manifest_file_number();
}

uint64_t DBImpl::TEST_Current_Next_FileNo() {
  return versions_->current_next_file_number();
}

Status DBImpl::TEST_CompactRange(int level, const Slice* begin,
                                 const Slice* end,
                                 ColumnFamilyHandle* column_family,
                                 bool disallow_trivial_move) {
  ColumnFamilyData* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<ColumnFamilyHandleImpl*>(column_family);
    cfd = cfh->cfd();
  }
  int output_level =
      (cfd->ioptions()->compaction_style == kCompactionStyleUniversal ||
       cfd->ioptions()->compaction_style == kCompactionStyleFIFO)
          ? level
          : level + 1;
  return RunManualCompaction(cfd, level, output_level, 0, 0, begin, end, true,
                             disallow_trivial_move);
}

Status DBImpl::TEST_SwitchMemtable(ColumnFamilyData* cfd) {
  WriteContext write_context;
  InstrumentedMutexLock l(&mutex_);
  if (cfd == nullptr) {
    cfd = default_cf_handle_->cfd();
  }
  return SwitchMemtable(cfd, &write_context);
}

Status DBImpl::TEST_FlushMemTable(bool wait, ColumnFamilyHandle* cfh) {
  FlushOptions fo;
  fo.wait = wait;
  ColumnFamilyData* cfd;
  if (cfh == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfhi = reinterpret_cast<ColumnFamilyHandleImpl*>(cfh);
    cfd = cfhi->cfd();
  }
  return FlushMemTable(cfd, fo, FlushReason::kTest);
}

Status DBImpl::TEST_WaitForFlushMemTable(ColumnFamilyHandle* column_family) {
  ColumnFamilyData* cfd;
  if (column_family == nullptr) {
    cfd = default_cf_handle_->cfd();
  } else {
    auto cfh = reinterpret_cast<ColumnFamilyHandleImpl*>(column_family);
    cfd = cfh->cfd();
  }
  return WaitForFlushMemTable(cfd);
}

Status DBImpl::TEST_WaitForCompact(bool wait_unscheduled) {
  // Wait until the compaction completes

  // TODO: a bug here. This function actually does not necessarily
  // wait for compact. It actually waits for scheduled compaction
  // OR flush to finish.

  InstrumentedMutexLock l(&mutex_);
  while ((bg_bottom_compaction_scheduled_ || bg_compaction_scheduled_ ||
          bg_flush_scheduled_ ||
          (wait_unscheduled && unscheduled_compactions_)) &&
         bg_error_.ok()) {
    bg_cv_.Wait();
  }
  return bg_error_;
}

void DBImpl::TEST_LockMutex() {
  mutex_.Lock();
}

void DBImpl::TEST_UnlockMutex() {
  mutex_.Unlock();
}

void* DBImpl::TEST_BeginWrite() {
  auto w = new WriteThread::Writer();
  write_thread_.EnterUnbatched(w, &mutex_);
  return reinterpret_cast<void*>(w);
}

void DBImpl::TEST_EndWrite(void* w) {
  auto writer = reinterpret_cast<WriteThread::Writer*>(w);
  write_thread_.ExitUnbatched(writer);
  delete writer;
}

size_t DBImpl::TEST_LogsToFreeSize() {
  InstrumentedMutexLock l(&mutex_);
  return logs_to_free_.size();
}

uint64_t DBImpl::TEST_LogfileNumber() {
  InstrumentedMutexLock l(&mutex_);
  return logfile_number_;
}

Status DBImpl::TEST_GetAllImmutableCFOptions(
    std::unordered_map<std::string, const ImmutableCFOptions*>* iopts_map) {
  std::vector<std::string> cf_names;
  std::vector<const ImmutableCFOptions*> iopts;
  {
    InstrumentedMutexLock l(&mutex_);
    for (auto cfd : *versions_->GetColumnFamilySet()) {
      cf_names.push_back(cfd->GetName());
      iopts.push_back(cfd->ioptions());
    }
  }
  iopts_map->clear();
  for (size_t i = 0; i < cf_names.size(); ++i) {
    iopts_map->insert({cf_names[i], iopts[i]});
  }

  return Status::OK();
}

uint64_t DBImpl::TEST_FindMinLogContainingOutstandingPrep() {
  return logs_with_prep_tracker_.FindMinLogContainingOutstandingPrep();
}

size_t DBImpl::TEST_PreparedSectionCompletedSize() {
  return logs_with_prep_tracker_.TEST_PreparedSectionCompletedSize();
}

size_t DBImpl::TEST_LogsWithPrepSize() {
  return logs_with_prep_tracker_.TEST_LogsWithPrepSize();
}

uint64_t DBImpl::TEST_FindMinPrepLogReferencedByMemTable() {
  autovector<MemTable*> empty_list;
  return FindMinPrepLogReferencedByMemTable(versions_.get(), nullptr,
                                            empty_list);
}

Status DBImpl::TEST_GetLatestMutableCFOptions(
    ColumnFamilyHandle* column_family, MutableCFOptions* mutable_cf_options) {
  InstrumentedMutexLock l(&mutex_);

  auto cfh = reinterpret_cast<ColumnFamilyHandleImpl*>(column_family);
  *mutable_cf_options = *cfh->cfd()->GetLatestMutableCFOptions();
  return Status::OK();
}

int DBImpl::TEST_BGCompactionsAllowed() const {
  InstrumentedMutexLock l(&mutex_);
  return GetBGJobLimits().max_compactions;
}

int DBImpl::TEST_BGFlushesAllowed() const {
  InstrumentedMutexLock l(&mutex_);
  return GetBGJobLimits().max_flushes;
}

SequenceNumber DBImpl::TEST_GetLastVisibleSequence() const {
  if (last_seq_same_as_publish_seq_) {
    return versions_->LastSequence();
  } else {
    return versions_->LastAllocatedSequence();
  }
}

}  // namespace rocksdb
#endif  // NDEBUG
