//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).
//

#ifndef ROCKSDB_LITE

#include "rocksdb/convenience.h"

#include "db/db_impl/db_impl.h"
#include "util/cast_util.h"

namespace ROCKSDB_NAMESPACE {

void CancelAllBackgroundWork(DB* db, bool wait) {
  (static_cast_with_check<DBImpl>(db->GetRootDB()))
      ->CancelAllBackgroundWork(wait);
}

Status DeleteFilesInRange(DB* db, ColumnFamilyHandle* column_family,
                          const Slice* begin, const Slice* end,
                          bool include_end) {
  RangePtr range(begin, end);
  return DeleteFilesInRanges(db, column_family, &range, 1, include_end);
}

Status DeleteFilesInRanges(DB* db, ColumnFamilyHandle* column_family,
                           const RangePtr* ranges, size_t n,
                           bool include_end) {
  return (static_cast_with_check<DBImpl>(db->GetRootDB()))
      ->DeleteFilesInRanges(column_family, ranges, n, include_end);
}

Status VerifySstFileChecksum(const Options& options,
                             const EnvOptions& env_options,
                             const std::string& file_path) {
  return VerifySstFileChecksum(options, env_options, ReadOptions(), file_path);
}
Status VerifySstFileChecksum(const Options& options,
                             const EnvOptions& env_options,
                             const ReadOptions& read_options,
                             const std::string& file_path) {
  std::unique_ptr<FSRandomAccessFile> file;
  uint64_t file_size;
  InternalKeyComparator internal_comparator(options.comparator);
  ImmutableOptions ioptions(options);

  Status s = ioptions.fs->NewRandomAccessFile(file_path,
                                              FileOptions(env_options),
                                              &file, nullptr);
  if (s.ok()) {
    s = ioptions.fs->GetFileSize(file_path, IOOptions(), &file_size, nullptr);
  } else {
    return s;
  }
  std::unique_ptr<TableReader> table_reader;
  std::unique_ptr<RandomAccessFileReader> file_reader(
      new RandomAccessFileReader(std::move(file), file_path));
  const bool kImmortal = true;
  s = ioptions.table_factory->NewTableReader(
      TableReaderOptions(ioptions, options.prefix_extractor, env_options,
                         internal_comparator, false /* skip_filters */,
                         !kImmortal, false /* force_direct_prefetch */,
                         -1 /* level */),
      std::move(file_reader), file_size, &table_reader,
      false /* prefetch_index_and_filter_in_cache */);
  if (!s.ok()) {
    return s;
  }
  s = table_reader->VerifyChecksum(read_options,
                                   TableReaderCaller::kUserVerifyChecksum);
  return s;
}

}  // namespace ROCKSDB_NAMESPACE

#endif  // ROCKSDB_LITE
