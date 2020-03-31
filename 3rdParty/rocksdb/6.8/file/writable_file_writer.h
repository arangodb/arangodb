//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once
#include <atomic>
#include <string>
#include "db/version_edit.h"
#include "port/port.h"
#include "rocksdb/env.h"
#include "rocksdb/file_checksum.h"
#include "rocksdb/file_system.h"
#include "rocksdb/listener.h"
#include "rocksdb/rate_limiter.h"
#include "test_util/sync_point.h"
#include "util/aligned_buffer.h"

namespace ROCKSDB_NAMESPACE {
class Statistics;

// WritableFileWriter is a wrapper on top of Env::WritableFile. It provides
// facilities to:
// - Handle Buffered and Direct writes.
// - Rate limit writes.
// - Flush and Sync the data to the underlying filesystem.
// - Notify any interested listeners on the completion of a write.
// - Update IO stats.
class WritableFileWriter {
 private:
#ifndef ROCKSDB_LITE
  void NotifyOnFileWriteFinish(uint64_t offset, size_t length,
                               const FileOperationInfo::TimePoint& start_ts,
                               const FileOperationInfo::TimePoint& finish_ts,
                               const Status& status) {
    FileOperationInfo info(file_name_, start_ts, finish_ts);
    info.offset = offset;
    info.length = length;
    info.status = status;

    for (auto& listener : listeners_) {
      listener->OnFileWriteFinish(info);
    }
  }
#endif  // ROCKSDB_LITE

  bool ShouldNotifyListeners() const { return !listeners_.empty(); }
  void CalculateFileChecksum(const Slice& data);

  std::unique_ptr<FSWritableFile> writable_file_;
  std::string file_name_;
  Env* env_;
  AlignedBuffer buf_;
  size_t max_buffer_size_;
  // Actually written data size can be used for truncate
  // not counting padding data
  uint64_t filesize_;
#ifndef ROCKSDB_LITE
  // This is necessary when we use unbuffered access
  // and writes must happen on aligned offsets
  // so we need to go back and write that page again
  uint64_t next_write_offset_;
#endif  // ROCKSDB_LITE
  bool pending_sync_;
  uint64_t last_sync_size_;
  uint64_t bytes_per_sync_;
  RateLimiter* rate_limiter_;
  Statistics* stats_;
  std::vector<std::shared_ptr<EventListener>> listeners_;
  FileChecksumFunc* checksum_func_;
  std::string file_checksum_ = kUnknownFileChecksum;
  bool is_first_checksum_ = true;

 public:
  WritableFileWriter(
      std::unique_ptr<FSWritableFile>&& file, const std::string& _file_name,
      const FileOptions& options, Env* env = nullptr,
      Statistics* stats = nullptr,
      const std::vector<std::shared_ptr<EventListener>>& listeners = {},
      FileChecksumFunc* checksum_func = nullptr)
      : writable_file_(std::move(file)),
        file_name_(_file_name),
        env_(env),
        buf_(),
        max_buffer_size_(options.writable_file_max_buffer_size),
        filesize_(0),
#ifndef ROCKSDB_LITE
        next_write_offset_(0),
#endif  // ROCKSDB_LITE
        pending_sync_(false),
        last_sync_size_(0),
        bytes_per_sync_(options.bytes_per_sync),
        rate_limiter_(options.rate_limiter),
        stats_(stats),
        listeners_(),
        checksum_func_(checksum_func) {
    TEST_SYNC_POINT_CALLBACK("WritableFileWriter::WritableFileWriter:0",
                             reinterpret_cast<void*>(max_buffer_size_));
    buf_.Alignment(writable_file_->GetRequiredBufferAlignment());
    buf_.AllocateNewBuffer(std::min((size_t)65536, max_buffer_size_));
#ifndef ROCKSDB_LITE
    std::for_each(listeners.begin(), listeners.end(),
                  [this](const std::shared_ptr<EventListener>& e) {
                    if (e->ShouldBeNotifiedOnFileIO()) {
                      listeners_.emplace_back(e);
                    }
                  });
#else  // !ROCKSDB_LITE
    (void)listeners;
#endif
  }

  WritableFileWriter(const WritableFileWriter&) = delete;

  WritableFileWriter& operator=(const WritableFileWriter&) = delete;

  ~WritableFileWriter() { Close(); }

  std::string file_name() const { return file_name_; }

  Status Append(const Slice& data);

  Status Pad(const size_t pad_bytes);

  Status Flush();

  Status Close();

  Status Sync(bool use_fsync);

  // Sync only the data that was already Flush()ed. Safe to call concurrently
  // with Append() and Flush(). If !writable_file_->IsSyncThreadSafe(),
  // returns NotSupported status.
  Status SyncWithoutFlush(bool use_fsync);

  uint64_t GetFileSize() const { return filesize_; }

  Status InvalidateCache(size_t offset, size_t length) {
    return writable_file_->InvalidateCache(offset, length);
  }

  FSWritableFile* writable_file() const { return writable_file_.get(); }

  bool use_direct_io() { return writable_file_->use_direct_io(); }

  bool TEST_BufferIsEmpty() { return buf_.CurrentSize() == 0; }

  void TEST_SetFileChecksumFunc(FileChecksumFunc* checksum_func) {
    checksum_func_ = checksum_func;
  }

  const std::string& GetFileChecksum() const { return file_checksum_; }

  const char* GetFileChecksumFuncName() const;

 private:
  // Used when os buffering is OFF and we are writing
  // DMA such as in Direct I/O mode
#ifndef ROCKSDB_LITE
  Status WriteDirect();
#endif  // !ROCKSDB_LITE
  // Normal write
  Status WriteBuffered(const char* data, size_t size);
  Status RangeSync(uint64_t offset, uint64_t nbytes);
  Status SyncInternal(bool use_fsync);
};
}  // namespace ROCKSDB_NAMESPACE
