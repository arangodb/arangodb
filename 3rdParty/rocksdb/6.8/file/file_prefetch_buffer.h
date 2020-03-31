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
#include <sstream>
#include <string>
#include "file/random_access_file_reader.h"
#include "port/port.h"
#include "rocksdb/env.h"
#include "util/aligned_buffer.h"

namespace ROCKSDB_NAMESPACE {

// FilePrefetchBuffer is a smart buffer to store and read data from a file.
class FilePrefetchBuffer {
 public:
  // Constructor.
  //
  // All arguments are optional.
  // file_reader        : the file reader to use. Can be a nullptr.
  // readahead_size     : the initial readahead size.
  // max_readahead_size : the maximum readahead size.
  //   If max_readahead_size > readahead_size, the readahead size will be
  //   doubled on every IO until max_readahead_size is hit.
  //   Typically this is set as a multiple of readahead_size.
  //   max_readahead_size should be greater than equal to readahead_size.
  // enable : controls whether reading from the buffer is enabled.
  //   If false, TryReadFromCache() always return false, and we only take stats
  //   for the minimum offset if track_min_offset = true.
  // track_min_offset : Track the minimum offset ever read and collect stats on
  //   it. Used for adaptable readahead of the file footer/metadata.
  //
  // Automatic readhead is enabled for a file if file_reader, readahead_size,
  // and max_readahead_size are passed in.
  // If file_reader is a nullptr, setting readadhead_size and max_readahead_size
  // does not make any sense. So it does nothing.
  // A user can construct a FilePrefetchBuffer without any arguments, but use
  // `Prefetch` to load data into the buffer.
  FilePrefetchBuffer(RandomAccessFileReader* file_reader = nullptr,
                     size_t readadhead_size = 0, size_t max_readahead_size = 0,
                     bool enable = true, bool track_min_offset = false)
      : buffer_offset_(0),
        file_reader_(file_reader),
        readahead_size_(readadhead_size),
        max_readahead_size_(max_readahead_size),
        min_offset_read_(port::kMaxSizet),
        enable_(enable),
        track_min_offset_(track_min_offset) {}

  // Load data into the buffer from a file.
  // reader : the file reader.
  // offset : the file offset to start reading from.
  // n      : the number of bytes to read.
  // for_compaction : if prefetch is done for compaction read.
  Status Prefetch(RandomAccessFileReader* reader, uint64_t offset, size_t n,
                  bool for_compaction = false);

  // Tries returning the data for a file raed from this buffer, if that data is
  // in the buffer.
  // It handles tracking the minimum read offset if track_min_offset = true.
  // It also does the exponential readahead when readadhead_size is set as part
  // of the constructor.
  //
  // offset : the file offset.
  // n      : the number of bytes.
  // result : output buffer to put the data into.
  // for_compaction : if cache read is done for compaction read.
  bool TryReadFromCache(uint64_t offset, size_t n, Slice* result,
                        bool for_compaction = false);

  // The minimum `offset` ever passed to TryReadFromCache(). This will nly be
  // tracked if track_min_offset = true.
  size_t min_offset_read() const { return min_offset_read_; }

 private:
  AlignedBuffer buffer_;
  uint64_t buffer_offset_;
  RandomAccessFileReader* file_reader_;
  size_t readahead_size_;
  size_t max_readahead_size_;
  // The minimum `offset` ever passed to TryReadFromCache().
  size_t min_offset_read_;
  // if false, TryReadFromCache() always return false, and we only take stats
  // for track_min_offset_ if track_min_offset_ = true
  bool enable_;
  // If true, track minimum `offset` ever passed to TryReadFromCache(), which
  // can be fetched from min_offset_read().
  bool track_min_offset_;
};
}  // namespace ROCKSDB_NAMESPACE
