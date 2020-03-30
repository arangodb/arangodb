//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//
#pragma once
#include <string>

#include "file/filename.h"
#include "options/db_options.h"
#include "rocksdb/env.h"
#include "rocksdb/file_system.h"
#include "rocksdb/status.h"
#include "rocksdb/types.h"

namespace ROCKSDB_NAMESPACE {
// use_fsync maps to options.use_fsync, which determines the way that
// the file is synced after copying.
extern Status CopyFile(FileSystem* fs, const std::string& source,
                       const std::string& destination, uint64_t size,
                       bool use_fsync);

extern Status CreateFile(FileSystem* fs, const std::string& destination,
                         const std::string& contents, bool use_fsync);

extern Status DeleteDBFile(const ImmutableDBOptions* db_options,
                           const std::string& fname,
                           const std::string& path_to_sync, const bool force_bg,
                           const bool force_fg);

extern bool IsWalDirSameAsDBPath(const ImmutableDBOptions* db_options);

}  // namespace ROCKSDB_NAMESPACE
