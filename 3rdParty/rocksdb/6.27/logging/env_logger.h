//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Logger implementation that uses custom Env object for logging.

#pragma once

#include <time.h>
#include <atomic>
#include <memory>
#include "port/sys_time.h"

#include "file/writable_file_writer.h"
#include "monitoring/iostats_context_imp.h"
#include "rocksdb/env.h"
#include "rocksdb/slice.h"
#include "test_util/sync_point.h"
#include "util/mutexlock.h"

namespace ROCKSDB_NAMESPACE {

class EnvLogger : public Logger {
 public:
  EnvLogger(std::unique_ptr<FSWritableFile>&& writable_file,
            const std::string& fname, const EnvOptions& options, Env* env,
            InfoLogLevel log_level = InfoLogLevel::ERROR_LEVEL)
      : Logger(log_level),
        env_(env),
        clock_(env_->GetSystemClock().get()),
        file_(std::move(writable_file), fname, options, clock_),
        last_flush_micros_(0),
        flush_pending_(false) {}

  ~EnvLogger() {
    if (!closed_) {
      closed_ = true;
      CloseHelper().PermitUncheckedError();
    }
  }

 private:
  void FlushLocked() {
    mutex_.AssertHeld();
    if (flush_pending_) {
      flush_pending_ = false;
      file_.Flush().PermitUncheckedError();
    }
    last_flush_micros_ = clock_->NowMicros();
  }

  void Flush() override {
    TEST_SYNC_POINT("EnvLogger::Flush:Begin1");
    TEST_SYNC_POINT("EnvLogger::Flush:Begin2");

    MutexLock l(&mutex_);
    FlushLocked();
  }

  Status CloseImpl() override { return CloseHelper(); }

  Status CloseHelper() {
    mutex_.Lock();
    const auto close_status = file_.Close();
    mutex_.Unlock();

    if (close_status.ok()) {
      return close_status;
    }
    return Status::IOError("Close of log file failed with error:" +
                           (close_status.getState()
                                ? std::string(close_status.getState())
                                : std::string()));
  }

  using Logger::Logv;
  void Logv(const char* format, va_list ap) override {
    IOSTATS_TIMER_GUARD(logger_nanos);

    const uint64_t thread_id = env_->GetThreadID();

    // We try twice: the first time with a fixed-size stack allocated buffer,
    // and the second time with a much larger dynamically allocated buffer.
    char buffer[500];
    for (int iter = 0; iter < 2; iter++) {
      char* base;
      int bufsize;
      if (iter == 0) {
        bufsize = sizeof(buffer);
        base = buffer;
      } else {
        bufsize = 65536;
        base = new char[bufsize];
      }
      char* p = base;
      char* limit = base + bufsize;

      struct timeval now_tv;
      gettimeofday(&now_tv, nullptr);
      const time_t seconds = now_tv.tv_sec;
      struct tm t;
      localtime_r(&seconds, &t);
      p += snprintf(p, limit - p, "%04d/%02d/%02d-%02d:%02d:%02d.%06d %llx ",
                    t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour,
                    t.tm_min, t.tm_sec, static_cast<int>(now_tv.tv_usec),
                    static_cast<long long unsigned int>(thread_id));

      // Print the message
      if (p < limit) {
        va_list backup_ap;
        va_copy(backup_ap, ap);
        p += vsnprintf(p, limit - p, format, backup_ap);
        va_end(backup_ap);
      }

      // Truncate to available space if necessary
      if (p >= limit) {
        if (iter == 0) {
          continue;  // Try again with larger buffer
        } else {
          p = limit - 1;
        }
      }

      // Add newline if necessary
      if (p == base || p[-1] != '\n') {
        *p++ = '\n';
      }

      assert(p <= limit);
      mutex_.Lock();
      // We will ignore any error returned by Append().
      file_.Append(Slice(base, p - base)).PermitUncheckedError();
      flush_pending_ = true;
      const uint64_t now_micros = clock_->NowMicros();
      if (now_micros - last_flush_micros_ >= flush_every_seconds_ * 1000000) {
        FlushLocked();
      }
      mutex_.Unlock();
      if (base != buffer) {
        delete[] base;
      }
      break;
    }
  }

  size_t GetLogFileSize() const override {
    MutexLock l(&mutex_);
    return file_.GetFileSize();
  }

 private:
  Env* env_;
  SystemClock* clock_;
  WritableFileWriter file_;
  mutable port::Mutex mutex_;  // Mutex to protect the shared variables below.
  const static uint64_t flush_every_seconds_ = 5;
  std::atomic_uint_fast64_t last_flush_micros_;
  std::atomic<bool> flush_pending_;
};

}  // namespace ROCKSDB_NAMESPACE
