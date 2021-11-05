//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/compaction/compaction_picker_fifo.h"
#ifndef ROCKSDB_LITE

#include <cinttypes>
#include <string>
#include <vector>

#include "db/column_family.h"
#include "logging/log_buffer.h"
#include "logging/logging.h"
#include "util/string_util.h"

namespace ROCKSDB_NAMESPACE {
namespace {
uint64_t GetTotalFilesSize(const std::vector<FileMetaData*>& files) {
  uint64_t total_size = 0;
  for (const auto& f : files) {
    total_size += f->fd.file_size;
  }
  return total_size;
}
}  // anonymous namespace

bool FIFOCompactionPicker::NeedsCompaction(
    const VersionStorageInfo* vstorage) const {
  const int kLevel0 = 0;
  return vstorage->CompactionScore(kLevel0) >= 1;
}

Compaction* FIFOCompactionPicker::PickTTLCompaction(
    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
    LogBuffer* log_buffer) {
  assert(mutable_cf_options.ttl > 0);

  const int kLevel0 = 0;
  const std::vector<FileMetaData*>& level_files = vstorage->LevelFiles(kLevel0);
  uint64_t total_size = GetTotalFilesSize(level_files);

  int64_t _current_time;
  auto status = ioptions_.clock->GetCurrentTime(&_current_time);
  if (!status.ok()) {
    ROCKS_LOG_BUFFER(log_buffer,
                     "[%s] FIFO compaction: Couldn't get current time: %s. "
                     "Not doing compactions based on TTL. ",
                     cf_name.c_str(), status.ToString().c_str());
    return nullptr;
  }
  const uint64_t current_time = static_cast<uint64_t>(_current_time);

  if (!level0_compactions_in_progress_.empty()) {
    ROCKS_LOG_BUFFER(
        log_buffer,
        "[%s] FIFO compaction: Already executing compaction. No need "
        "to run parallel compactions since compactions are very fast",
        cf_name.c_str());
    return nullptr;
  }

  std::vector<CompactionInputFiles> inputs;
  inputs.emplace_back();
  inputs[0].level = 0;

  // avoid underflow
  if (current_time > mutable_cf_options.ttl) {
    for (auto ritr = level_files.rbegin(); ritr != level_files.rend(); ++ritr) {
      FileMetaData* f = *ritr;
      assert(f);
      if (f->fd.table_reader && f->fd.table_reader->GetTableProperties()) {
        uint64_t creation_time =
            f->fd.table_reader->GetTableProperties()->creation_time;
        if (creation_time == 0 ||
            creation_time >= (current_time - mutable_cf_options.ttl)) {
          break;
        }
      }
      total_size -= f->compensated_file_size;
      inputs[0].files.push_back(f);
    }
  }

  // Return a nullptr and proceed to size-based FIFO compaction if:
  // 1. there are no files older than ttl OR
  // 2. there are a few files older than ttl, but deleting them will not bring
  //    the total size to be less than max_table_files_size threshold.
  if (inputs[0].files.empty() ||
      total_size >
          mutable_cf_options.compaction_options_fifo.max_table_files_size) {
    return nullptr;
  }

  for (const auto& f : inputs[0].files) {
    uint64_t creation_time = 0;
    assert(f);
    if (f->fd.table_reader && f->fd.table_reader->GetTableProperties()) {
      creation_time = f->fd.table_reader->GetTableProperties()->creation_time;
    }
    ROCKS_LOG_BUFFER(log_buffer,
                     "[%s] FIFO compaction: picking file %" PRIu64
                     " with creation time %" PRIu64 " for deletion",
                     cf_name.c_str(), f->fd.GetNumber(), creation_time);
  }

  Compaction* c = new Compaction(
      vstorage, ioptions_, mutable_cf_options, mutable_db_options,
      std::move(inputs), 0, 0, 0, 0, kNoCompression,
      mutable_cf_options.compression_opts, Temperature::kUnknown,
      /* max_subcompactions */ 0, {}, /* is manual */ false,
      vstorage->CompactionScore(0),
      /* is deletion compaction */ true, CompactionReason::kFIFOTtl);
  return c;
}

Compaction* FIFOCompactionPicker::PickSizeCompaction(
    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
    LogBuffer* log_buffer) {
  const int kLevel0 = 0;
  const std::vector<FileMetaData*>& level_files = vstorage->LevelFiles(kLevel0);
  uint64_t total_size = GetTotalFilesSize(level_files);

  if (total_size <=
          mutable_cf_options.compaction_options_fifo.max_table_files_size ||
      level_files.size() == 0) {
    // total size not exceeded
    if (mutable_cf_options.compaction_options_fifo.allow_compaction &&
        level_files.size() > 0) {
      CompactionInputFiles comp_inputs;
      // try to prevent same files from being compacted multiple times, which
      // could produce large files that may never TTL-expire. Achieve this by
      // disallowing compactions with files larger than memtable (inflate its
      // size by 10% to account for uncompressed L0 files that may have size
      // slightly greater than memtable size limit).
      size_t max_compact_bytes_per_del_file =
          static_cast<size_t>(MultiplyCheckOverflow(
              static_cast<uint64_t>(mutable_cf_options.write_buffer_size),
              1.1));
      if (FindIntraL0Compaction(
              level_files,
              mutable_cf_options
                  .level0_file_num_compaction_trigger /* min_files_to_compact */
              ,
              max_compact_bytes_per_del_file,
              mutable_cf_options.max_compaction_bytes, &comp_inputs)) {
        Compaction* c = new Compaction(
            vstorage, ioptions_, mutable_cf_options, mutable_db_options,
            {comp_inputs}, 0, 16 * 1024 * 1024 /* output file size limit */,
            0 /* max compaction bytes, not applicable */,
            0 /* output path ID */, mutable_cf_options.compression,
            mutable_cf_options.compression_opts, Temperature::kUnknown,
            0 /* max_subcompactions */, {},
            /* is manual */ false, vstorage->CompactionScore(0),
            /* is deletion compaction */ false,
            CompactionReason::kFIFOReduceNumFiles);
        return c;
      }
    }

    ROCKS_LOG_BUFFER(
        log_buffer,
        "[%s] FIFO compaction: nothing to do. Total size %" PRIu64
        ", max size %" PRIu64 "\n",
        cf_name.c_str(), total_size,
        mutable_cf_options.compaction_options_fifo.max_table_files_size);
    return nullptr;
  }

  if (!level0_compactions_in_progress_.empty()) {
    ROCKS_LOG_BUFFER(
        log_buffer,
        "[%s] FIFO compaction: Already executing compaction. No need "
        "to run parallel compactions since compactions are very fast",
        cf_name.c_str());
    return nullptr;
  }

  std::vector<CompactionInputFiles> inputs;
  inputs.emplace_back();
  inputs[0].level = 0;

  for (auto ritr = level_files.rbegin(); ritr != level_files.rend(); ++ritr) {
    auto f = *ritr;
    total_size -= f->compensated_file_size;
    inputs[0].files.push_back(f);
    char tmp_fsize[16];
    AppendHumanBytes(f->fd.GetFileSize(), tmp_fsize, sizeof(tmp_fsize));
    ROCKS_LOG_BUFFER(log_buffer,
                     "[%s] FIFO compaction: picking file %" PRIu64
                     " with size %s for deletion",
                     cf_name.c_str(), f->fd.GetNumber(), tmp_fsize);
    if (total_size <=
        mutable_cf_options.compaction_options_fifo.max_table_files_size) {
      break;
    }
  }

  Compaction* c = new Compaction(
      vstorage, ioptions_, mutable_cf_options, mutable_db_options,
      std::move(inputs), 0, 0, 0, 0, kNoCompression,
      mutable_cf_options.compression_opts, Temperature::kUnknown,
      /* max_subcompactions */ 0, {}, /* is manual */ false,
      vstorage->CompactionScore(0),
      /* is deletion compaction */ true, CompactionReason::kFIFOMaxSize);
  return c;
}

Compaction* FIFOCompactionPicker::PickCompactionToWarm(
    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
    LogBuffer* log_buffer) {
  if (mutable_cf_options.compaction_options_fifo.age_for_warm == 0) {
    return nullptr;
  }

  const int kLevel0 = 0;
  const std::vector<FileMetaData*>& level_files = vstorage->LevelFiles(kLevel0);

  int64_t _current_time;
  auto status = ioptions_.clock->GetCurrentTime(&_current_time);
  if (!status.ok()) {
    ROCKS_LOG_BUFFER(log_buffer,
                     "[%s] FIFO compaction: Couldn't get current time: %s. "
                     "Not doing compactions based on warm threshold. ",
                     cf_name.c_str(), status.ToString().c_str());
    return nullptr;
  }
  const uint64_t current_time = static_cast<uint64_t>(_current_time);

  if (!level0_compactions_in_progress_.empty()) {
    ROCKS_LOG_BUFFER(
        log_buffer,
        "[%s] FIFO compaction: Already executing compaction. Parallel "
        "compactions are not supported",
        cf_name.c_str());
    return nullptr;
  }

  std::vector<CompactionInputFiles> inputs;
  inputs.emplace_back();
  inputs[0].level = 0;

  // avoid underflow
  if (current_time > mutable_cf_options.compaction_options_fifo.age_for_warm) {
    uint64_t create_time_threshold =
        current_time - mutable_cf_options.compaction_options_fifo.age_for_warm;
    uint64_t compaction_size = 0;
    // We will ideally identify a file qualifying for warm tier by knowing
    // the timestamp for the youngest entry in the file. However, right now
    // we don't have the information. We infer it by looking at timestamp
    // of the next file's (which is just younger) oldest entry's timestamp.
    FileMetaData* prev_file = nullptr;
    for (auto ritr = level_files.rbegin(); ritr != level_files.rend(); ++ritr) {
      FileMetaData* f = *ritr;
      assert(f);
      if (f->being_compacted) {
        // Right now this probably won't happen as we never try to schedule
        // two compactions in parallel, so here we just simply don't schedule
        // anything.
        return nullptr;
      }
      uint64_t oldest_ancester_time = f->TryGetOldestAncesterTime();
      if (oldest_ancester_time == kUnknownOldestAncesterTime) {
        // Older files might not have enough information. It is possible to
        // handle these files by looking at newer files, but maintaining the
        // logic isn't worth it.
        break;
      }
      if (oldest_ancester_time > create_time_threshold) {
        // The previous file (which has slightly older data) doesn't qualify
        // for warm tier.
        break;
      }
      if (prev_file != nullptr) {
        compaction_size += prev_file->fd.GetFileSize();
        if (compaction_size > mutable_cf_options.max_compaction_bytes) {
          break;
        }
        inputs[0].files.push_back(prev_file);
        ROCKS_LOG_BUFFER(log_buffer,
                         "[%s] FIFO compaction: picking file %" PRIu64
                         " with next file's oldest time %" PRIu64 " for warm",
                         cf_name.c_str(), prev_file->fd.GetNumber(),
                         oldest_ancester_time);
      }
      if (f->temperature == Temperature::kUnknown ||
          f->temperature == Temperature::kHot) {
        prev_file = f;
      } else if (!inputs[0].files.empty()) {
        // A warm file newer than files picked.
        break;
      } else {
        assert(prev_file == nullptr);
      }
    }
  }

  if (inputs[0].files.empty()) {
    return nullptr;
  }

  Compaction* c = new Compaction(
      vstorage, ioptions_, mutable_cf_options, mutable_db_options,
      std::move(inputs), 0, 0 /* output file size limit */,
      0 /* max compaction bytes, not applicable */, 0 /* output path ID */,
      mutable_cf_options.compression, mutable_cf_options.compression_opts,
      Temperature::kWarm,
      /* max_subcompactions */ 0, {}, /* is manual */ false,
      vstorage->CompactionScore(0),
      /* is deletion compaction */ false, CompactionReason::kChangeTemperature);
  return c;
}

Compaction* FIFOCompactionPicker::PickCompaction(
    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
    LogBuffer* log_buffer, SequenceNumber /*earliest_memtable_seqno*/) {
  assert(vstorage->num_levels() == 1);

  Compaction* c = nullptr;
  if (mutable_cf_options.ttl > 0) {
    c = PickTTLCompaction(cf_name, mutable_cf_options, mutable_db_options,
                          vstorage, log_buffer);
  }
  if (c == nullptr) {
    c = PickSizeCompaction(cf_name, mutable_cf_options, mutable_db_options,
                           vstorage, log_buffer);
  }
  if (c == nullptr) {
    c = PickCompactionToWarm(cf_name, mutable_cf_options, mutable_db_options,
                             vstorage, log_buffer);
  }
  RegisterCompaction(c);
  return c;
}

Compaction* FIFOCompactionPicker::CompactRange(
    const std::string& cf_name, const MutableCFOptions& mutable_cf_options,
    const MutableDBOptions& mutable_db_options, VersionStorageInfo* vstorage,
    int input_level, int output_level,
    const CompactRangeOptions& /*compact_range_options*/,
    const InternalKey* /*begin*/, const InternalKey* /*end*/,
    InternalKey** compaction_end, bool* /*manual_conflict*/,
    uint64_t /*max_file_num_to_ignore*/) {
#ifdef NDEBUG
  (void)input_level;
  (void)output_level;
#endif
  assert(input_level == 0);
  assert(output_level == 0);
  *compaction_end = nullptr;
  LogBuffer log_buffer(InfoLogLevel::INFO_LEVEL, ioptions_.logger);
  Compaction* c = PickCompaction(cf_name, mutable_cf_options,
                                 mutable_db_options, vstorage, &log_buffer);
  log_buffer.FlushBufferToLog();
  return c;
}

}  // namespace ROCKSDB_NAMESPACE
#endif  // !ROCKSDB_LITE
