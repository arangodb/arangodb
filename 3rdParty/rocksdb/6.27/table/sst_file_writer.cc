//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#include "rocksdb/sst_file_writer.h"

#include <vector>

#include "db/db_impl/db_impl.h"
#include "db/dbformat.h"
#include "file/writable_file_writer.h"
#include "rocksdb/file_system.h"
#include "rocksdb/table.h"
#include "table/block_based/block_based_table_builder.h"
#include "table/sst_file_writer_collectors.h"
#include "test_util/sync_point.h"

namespace ROCKSDB_NAMESPACE {

const std::string ExternalSstFilePropertyNames::kVersion =
    "rocksdb.external_sst_file.version";
const std::string ExternalSstFilePropertyNames::kGlobalSeqno =
    "rocksdb.external_sst_file.global_seqno";

#ifndef ROCKSDB_LITE

const size_t kFadviseTrigger = 1024 * 1024; // 1MB

struct SstFileWriter::Rep {
  Rep(const EnvOptions& _env_options, const Options& options,
      Env::IOPriority _io_priority, const Comparator* _user_comparator,
      ColumnFamilyHandle* _cfh, bool _invalidate_page_cache, bool _skip_filters,
      std::string _db_session_id)
      : env_options(_env_options),
        ioptions(options),
        mutable_cf_options(options),
        io_priority(_io_priority),
        internal_comparator(_user_comparator),
        cfh(_cfh),
        invalidate_page_cache(_invalidate_page_cache),
        skip_filters(_skip_filters),
        db_session_id(_db_session_id) {}

  std::unique_ptr<WritableFileWriter> file_writer;
  std::unique_ptr<TableBuilder> builder;
  EnvOptions env_options;
  ImmutableOptions ioptions;
  MutableCFOptions mutable_cf_options;
  Env::IOPriority io_priority;
  InternalKeyComparator internal_comparator;
  ExternalSstFileInfo file_info;
  InternalKey ikey;
  std::string column_family_name;
  ColumnFamilyHandle* cfh;
  // If true, We will give the OS a hint that this file pages is not needed
  // every time we write 1MB to the file.
  bool invalidate_page_cache;
  // The size of the file during the last time we called Fadvise to remove
  // cached pages from page cache.
  uint64_t last_fadvise_size = 0;
  bool skip_filters;
  std::string db_session_id;
  uint64_t next_file_number = 1;

  Status AddImpl(const Slice& user_key, const Slice& value,
                 ValueType value_type) {
    if (!builder) {
      return Status::InvalidArgument("File is not opened");
    }

    if (file_info.num_entries == 0) {
      file_info.smallest_key.assign(user_key.data(), user_key.size());
    } else {
      if (internal_comparator.user_comparator()->Compare(
              user_key, file_info.largest_key) <= 0) {
        // Make sure that keys are added in order
        return Status::InvalidArgument(
            "Keys must be added in strict ascending order.");
      }
    }

    assert(value_type == kTypeValue || value_type == kTypeMerge ||
           value_type == kTypeDeletion ||
           value_type == kTypeDeletionWithTimestamp);

    constexpr SequenceNumber sequence_number = 0;

    ikey.Set(user_key, sequence_number, value_type);

    builder->Add(ikey.Encode(), value);

    // update file info
    file_info.num_entries++;
    file_info.largest_key.assign(user_key.data(), user_key.size());
    file_info.file_size = builder->FileSize();

    InvalidatePageCache(false /* closing */).PermitUncheckedError();
    return Status::OK();
  }

  Status Add(const Slice& user_key, const Slice& value, ValueType value_type) {
    if (internal_comparator.timestamp_size() != 0) {
      return Status::InvalidArgument("Timestamp size mismatch");
    }

    return AddImpl(user_key, value, value_type);
  }

  Status Add(const Slice& user_key, const Slice& timestamp, const Slice& value,
             ValueType value_type) {
    const size_t timestamp_size = timestamp.size();

    if (internal_comparator.timestamp_size() != timestamp_size) {
      return Status::InvalidArgument("Timestamp size mismatch");
    }

    const size_t user_key_size = user_key.size();

    if (user_key.data() + user_key_size == timestamp.data()) {
      Slice user_key_with_ts(user_key.data(), user_key_size + timestamp_size);
      return AddImpl(user_key_with_ts, value, value_type);
    }

    std::string user_key_with_ts;
    user_key_with_ts.reserve(user_key_size + timestamp_size);
    user_key_with_ts.append(user_key.data(), user_key_size);
    user_key_with_ts.append(timestamp.data(), timestamp_size);

    return AddImpl(user_key_with_ts, value, value_type);
  }

  Status DeleteRange(const Slice& begin_key, const Slice& end_key) {
    if (internal_comparator.timestamp_size() != 0) {
      return Status::InvalidArgument("Timestamp size mismatch");
    }

    if (!builder) {
      return Status::InvalidArgument("File is not opened");
    }

    RangeTombstone tombstone(begin_key, end_key, 0 /* Sequence Number */);
    if (file_info.num_range_del_entries == 0) {
      file_info.smallest_range_del_key.assign(tombstone.start_key_.data(),
                                              tombstone.start_key_.size());
      file_info.largest_range_del_key.assign(tombstone.end_key_.data(),
                                             tombstone.end_key_.size());
    } else {
      if (internal_comparator.user_comparator()->Compare(
              tombstone.start_key_, file_info.smallest_range_del_key) < 0) {
        file_info.smallest_range_del_key.assign(tombstone.start_key_.data(),
                                                tombstone.start_key_.size());
      }
      if (internal_comparator.user_comparator()->Compare(
              tombstone.end_key_, file_info.largest_range_del_key) > 0) {
        file_info.largest_range_del_key.assign(tombstone.end_key_.data(),
                                               tombstone.end_key_.size());
      }
    }

    auto ikey_and_end_key = tombstone.Serialize();
    builder->Add(ikey_and_end_key.first.Encode(), ikey_and_end_key.second);

    // update file info
    file_info.num_range_del_entries++;
    file_info.file_size = builder->FileSize();

    InvalidatePageCache(false /* closing */).PermitUncheckedError();
    return Status::OK();
  }

  Status InvalidatePageCache(bool closing) {
    Status s = Status::OK();
    if (invalidate_page_cache == false) {
      // Fadvise disabled
      return s;
    }
    uint64_t bytes_since_last_fadvise =
      builder->FileSize() - last_fadvise_size;
    if (bytes_since_last_fadvise > kFadviseTrigger || closing) {
      TEST_SYNC_POINT_CALLBACK("SstFileWriter::Rep::InvalidatePageCache",
                               &(bytes_since_last_fadvise));
      // Tell the OS that we don't need this file in page cache
      s = file_writer->InvalidateCache(0, 0);
      if (s.IsNotSupported()) {
        // NotSupported is fine as it could be a file type that doesn't use page
        // cache.
        s = Status::OK();
      }
      last_fadvise_size = builder->FileSize();
    }
    return s;
  }
};

SstFileWriter::SstFileWriter(const EnvOptions& env_options,
                             const Options& options,
                             const Comparator* user_comparator,
                             ColumnFamilyHandle* column_family,
                             bool invalidate_page_cache,
                             Env::IOPriority io_priority, bool skip_filters)
    : rep_(new Rep(env_options, options, io_priority, user_comparator,
                   column_family, invalidate_page_cache, skip_filters,
                   DBImpl::GenerateDbSessionId(options.env))) {
  // SstFileWriter is used to create sst files that can be added to database
  // later. Therefore, no real db_id and db_session_id are associated with it.
  // Here we mimic the way db_session_id behaves by getting a db_session_id
  // for each SstFileWriter, and (later below) assign unique file numbers
  // in the table properties. The db_id is set to be "SST Writer" for clarity.

  rep_->file_info.file_size = 0;
}

SstFileWriter::~SstFileWriter() {
  if (rep_->builder) {
    // User did not call Finish() or Finish() failed, we need to
    // abandon the builder.
    rep_->builder->Abandon();
  }
}

Status SstFileWriter::Open(const std::string& file_path) {
  Rep* r = rep_.get();
  Status s;
  std::unique_ptr<FSWritableFile> sst_file;
  FileOptions cur_file_opts(r->env_options);
  s = r->ioptions.env->GetFileSystem()->NewWritableFile(
      file_path, cur_file_opts, &sst_file, nullptr);
  if (!s.ok()) {
    return s;
  }

  sst_file->SetIOPriority(r->io_priority);

  CompressionType compression_type;
  CompressionOptions compression_opts;
  if (r->mutable_cf_options.bottommost_compression !=
      kDisableCompressionOption) {
    compression_type = r->mutable_cf_options.bottommost_compression;
    if (r->mutable_cf_options.bottommost_compression_opts.enabled) {
      compression_opts = r->mutable_cf_options.bottommost_compression_opts;
    } else {
      compression_opts = r->mutable_cf_options.compression_opts;
    }
  } else if (!r->ioptions.compression_per_level.empty()) {
    // Use the compression of the last level if we have per level compression
    compression_type = *(r->ioptions.compression_per_level.rbegin());
    compression_opts = r->mutable_cf_options.compression_opts;
  } else {
    compression_type = r->mutable_cf_options.compression;
    compression_opts = r->mutable_cf_options.compression_opts;
  }

  IntTblPropCollectorFactories int_tbl_prop_collector_factories;

  // SstFileWriter properties collector to add SstFileWriter version.
  int_tbl_prop_collector_factories.emplace_back(
      new SstFileWriterPropertiesCollectorFactory(2 /* version */,
                                                  0 /* global_seqno*/));

  // User collector factories
  auto user_collector_factories =
      r->ioptions.table_properties_collector_factories;
  for (size_t i = 0; i < user_collector_factories.size(); i++) {
    int_tbl_prop_collector_factories.emplace_back(
        new UserKeyTablePropertiesCollectorFactory(
            user_collector_factories[i]));
  }
  int unknown_level = -1;
  uint32_t cf_id;

  if (r->cfh != nullptr) {
    // user explicitly specified that this file will be ingested into cfh,
    // we can persist this information in the file.
    cf_id = r->cfh->GetID();
    r->column_family_name = r->cfh->GetName();
  } else {
    r->column_family_name = "";
    cf_id = TablePropertiesCollectorFactory::Context::kUnknownColumnFamily;
  }
  TableBuilderOptions table_builder_options(
      r->ioptions, r->mutable_cf_options, r->internal_comparator,
      &int_tbl_prop_collector_factories, compression_type, compression_opts,
      cf_id, r->column_family_name, unknown_level, false /* is_bottommost */,
      TableFileCreationReason::kMisc, 0 /* creation_time */,
      0 /* oldest_key_time */, 0 /* file_creation_time */,
      "SST Writer" /* db_id */, r->db_session_id, 0 /* target_file_size */,
      r->next_file_number);
  // External SST files used to each get a unique session id. Now for
  // slightly better uniqueness probability in constructing cache keys, we
  // assign fake file numbers to each file (into table properties) and keep
  // the same session id for the life of the SstFileWriter.
  r->next_file_number++;
  // XXX: when we can remove skip_filters from the SstFileWriter public API
  // we can remove it from TableBuilderOptions.
  table_builder_options.skip_filters = r->skip_filters;
  FileTypeSet tmp_set = r->ioptions.checksum_handoff_file_types;
  r->file_writer.reset(new WritableFileWriter(
      std::move(sst_file), file_path, r->env_options, r->ioptions.clock,
      nullptr /* io_tracer */, nullptr /* stats */, r->ioptions.listeners,
      r->ioptions.file_checksum_gen_factory.get(),
      tmp_set.Contains(FileType::kTableFile), false));

  // TODO(tec) : If table_factory is using compressed block cache, we will
  // be adding the external sst file blocks into it, which is wasteful.
  r->builder.reset(r->ioptions.table_factory->NewTableBuilder(
      table_builder_options, r->file_writer.get()));

  r->file_info = ExternalSstFileInfo();
  r->file_info.file_path = file_path;
  r->file_info.version = 2;
  return s;
}

Status SstFileWriter::Add(const Slice& user_key, const Slice& value) {
  return rep_->Add(user_key, value, ValueType::kTypeValue);
}

Status SstFileWriter::Put(const Slice& user_key, const Slice& value) {
  return rep_->Add(user_key, value, ValueType::kTypeValue);
}

Status SstFileWriter::Put(const Slice& user_key, const Slice& timestamp,
                          const Slice& value) {
  return rep_->Add(user_key, timestamp, value, ValueType::kTypeValue);
}

Status SstFileWriter::Merge(const Slice& user_key, const Slice& value) {
  return rep_->Add(user_key, value, ValueType::kTypeMerge);
}

Status SstFileWriter::Delete(const Slice& user_key) {
  return rep_->Add(user_key, Slice(), ValueType::kTypeDeletion);
}

Status SstFileWriter::Delete(const Slice& user_key, const Slice& timestamp) {
  return rep_->Add(user_key, timestamp, Slice(),
                   ValueType::kTypeDeletionWithTimestamp);
}

Status SstFileWriter::DeleteRange(const Slice& begin_key,
                                  const Slice& end_key) {
  return rep_->DeleteRange(begin_key, end_key);
}

Status SstFileWriter::Finish(ExternalSstFileInfo* file_info) {
  Rep* r = rep_.get();
  if (!r->builder) {
    return Status::InvalidArgument("File is not opened");
  }
  if (r->file_info.num_entries == 0 &&
      r->file_info.num_range_del_entries == 0) {
    return Status::InvalidArgument("Cannot create sst file with no entries");
  }

  Status s = r->builder->Finish();
  r->file_info.file_size = r->builder->FileSize();

  if (s.ok()) {
    s = r->file_writer->Sync(r->ioptions.use_fsync);
    r->InvalidatePageCache(true /* closing */).PermitUncheckedError();
    if (s.ok()) {
      s = r->file_writer->Close();
    }
  }
  if (s.ok()) {
    r->file_info.file_checksum = r->file_writer->GetFileChecksum();
    r->file_info.file_checksum_func_name =
        r->file_writer->GetFileChecksumFuncName();
  }
  if (!s.ok()) {
    r->ioptions.env->DeleteFile(r->file_info.file_path);
  }

  if (file_info != nullptr) {
    *file_info = r->file_info;
  }

  r->builder.reset();
  return s;
}

uint64_t SstFileWriter::FileSize() {
  return rep_->file_info.file_size;
}
#endif  // !ROCKSDB_LITE

}  // namespace ROCKSDB_NAMESPACE
