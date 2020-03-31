// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#pragma once

#include <string>

#include "rocksdb/rocksdb_namespace.h"

namespace ROCKSDB_NAMESPACE {

class Slice;

// A Comparator object provides a total order across slices that are
// used as keys in an sstable or a database.  A Comparator implementation
// must be thread-safe since rocksdb may invoke its methods concurrently
// from multiple threads.
class Comparator {
 public:
  Comparator() : timestamp_size_(0) {}

  Comparator(size_t ts_sz) : timestamp_size_(ts_sz) {}

  Comparator(const Comparator& orig) : timestamp_size_(orig.timestamp_size_) {}

  Comparator& operator=(const Comparator& rhs) {
    if (this != &rhs) {
      timestamp_size_ = rhs.timestamp_size_;
    }
    return *this;
  }

  virtual ~Comparator() {}

  static const char* Type() { return "Comparator"; }
  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  // Note that Compare(a, b) also compares timestamp if timestamp size is
  // non-zero. For the same user key with different timestamps, larger (newer)
  // timestamp comes first.
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

  // Compares two slices for equality. The following invariant should always
  // hold (and is the default implementation):
  //   Equal(a, b) iff Compare(a, b) == 0
  // Overwrite only if equality comparisons can be done more efficiently than
  // three-way comparisons.
  virtual bool Equal(const Slice& a, const Slice& b) const {
    return Compare(a, b) == 0;
  }

  // The name of the comparator.  Used to check for comparator
  // mismatches (i.e., a DB created with one comparator is
  // accessed using a different comparator.
  //
  // The client of this package should switch to a new name whenever
  // the comparator implementation changes in a way that will cause
  // the relative ordering of any two keys to change.
  //
  // Names starting with "rocksdb." are reserved and should not be used
  // by any clients of this package.
  virtual const char* Name() const = 0;

  // Advanced functions: these are used to reduce the space requirements
  // for internal data structures like index blocks.

  // If *start < limit, changes *start to a short string in [start,limit).
  // Simple comparator implementations may return with *start unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  virtual void FindShortestSeparator(std::string* start,
                                     const Slice& limit) const = 0;

  // Changes *key to a short string >= *key.
  // Simple comparator implementations may return with *key unchanged,
  // i.e., an implementation of this method that does nothing is correct.
  virtual void FindShortSuccessor(std::string* key) const = 0;

  // if it is a wrapped comparator, may return the root one.
  // return itself it is not wrapped.
  virtual const Comparator* GetRootComparator() const { return this; }

  // given two keys, determine if t is the successor of s
  virtual bool IsSameLengthImmediateSuccessor(const Slice& /*s*/,
                                              const Slice& /*t*/) const {
    return false;
  }

  // return true if two keys with different byte sequences can be regarded
  // as equal by this comparator.
  // The major use case is to determine if DataBlockHashIndex is compatible
  // with the customized comparator.
  virtual bool CanKeysWithDifferentByteContentsBeEqual() const { return true; }

  inline size_t timestamp_size() const { return timestamp_size_; }

  int CompareWithoutTimestamp(const Slice& a, const Slice& b) const {
    return CompareWithoutTimestamp(a, /*a_has_ts=*/true, b, /*b_has_ts=*/true);
  }

  // For two events e1 and e2 whose timestamps are t1 and t2 respectively,
  // Returns value:
  // < 0  iff t1 < t2
  // == 0 iff t1 == t2
  // > 0  iff t1 > t2
  // Note that an all-zero byte array will be the smallest (oldest) timestamp
  // of the same length.
  virtual int CompareTimestamp(const Slice& /*ts1*/,
                               const Slice& /*ts2*/) const {
    return 0;
  }

  virtual int CompareWithoutTimestamp(const Slice& a, bool /*a_has_ts*/,
                                      const Slice& b, bool /*b_has_ts*/) const {
    return Compare(a, b);
  }

 private:
  size_t timestamp_size_;
};

// Return a builtin comparator that uses lexicographic byte-wise
// ordering.  The result remains the property of this module and
// must not be deleted.
extern const Comparator* BytewiseComparator();

// Return a builtin comparator that uses reverse lexicographic byte-wise
// ordering.
extern const Comparator* ReverseBytewiseComparator();

}  // namespace ROCKSDB_NAMESPACE
