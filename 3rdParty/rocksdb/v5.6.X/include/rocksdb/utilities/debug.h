// Copyright (c) 2017-present, Facebook, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.
// This source code is also licensed under the GPLv2 license found in the
// COPYING file in the root directory of this source tree.

#pragma once

#ifndef ROCKSDB_LITE

#include "rocksdb/db.h"
#include "rocksdb/types.h"

namespace rocksdb {

// Data associated with a particular version of a key. A database may internally
// store multiple versions of a same user key due to snapshots, compaction not
// happening yet, etc.
struct KeyVersion {
  KeyVersion(const std::string& _user_key, const std::string& _value,
             SequenceNumber _sequence, int _type)
      : user_key(_user_key), value(_value), sequence(_sequence), type(_type) {}

  std::string user_key;
  std::string value;
  SequenceNumber sequence;
  // TODO(ajkr): we should provide a helper function that converts the int to a
  // string describing the type for easier debugging.
  int type;
};

// Returns listing of all versions of keys in the provided user key range.
// The range is inclusive-inclusive, i.e., [`begin_key`, `end_key`].
// The result is inserted into the provided vector, `key_versions`.
Status GetAllKeyVersions(DB* db, Slice begin_key, Slice end_key,
                         std::vector<KeyVersion>* key_versions);

}  // namespace rocksdb

#endif  // ROCKSDB_LITE
