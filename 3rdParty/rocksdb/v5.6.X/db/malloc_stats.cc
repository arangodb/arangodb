//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under the BSD-style license found in the
//  LICENSE file in the root directory of this source tree. An additional grant
//  of patent rights can be found in the PATENTS file in the same directory.
//
// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/malloc_stats.h"

#ifndef ROCKSDB_LITE
#include <memory>
#include <string.h>

namespace rocksdb {

#ifdef ROCKSDB_JEMALLOC
#include "jemalloc/jemalloc.h"

typedef struct {
  char* cur;
  char* end;
} MallocStatus;

static void GetJemallocStatus(void* mstat_arg, const char* status) {
  MallocStatus* mstat = reinterpret_cast<MallocStatus*>(mstat_arg);
  size_t status_len = status ? strlen(status) : 0;
  size_t buf_size = (size_t)(mstat->end - mstat->cur);
  if (!status_len || status_len > buf_size) {
    return;
  }

  snprintf(mstat->cur, buf_size, "%s", status);
  mstat->cur += status_len;
}
#endif  // ROCKSDB_JEMALLOC

void DumpMallocStats(std::string* stats) {
#ifdef ROCKSDB_JEMALLOC
  MallocStatus mstat;
  const unsigned int kMallocStatusLen = 1000000;
  std::unique_ptr<char[]> buf{new char[kMallocStatusLen + 1]};
  mstat.cur = buf.get();
  mstat.end = buf.get() + kMallocStatusLen;
  je_malloc_stats_print(GetJemallocStatus, &mstat, "");
  stats->append(buf.get());
#endif  // ROCKSDB_JEMALLOC
}

}
#endif  // !ROCKSDB_LITE
