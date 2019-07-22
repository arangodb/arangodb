//  Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#ifndef ROCKSDB_LITE

#include <string>
#include <vector>
#include "db/db_impl.h"

namespace rocksdb {

class LogReaderContainer {
 public:
  LogReaderContainer()
      : reader_(nullptr), reporter_(nullptr), status_(nullptr) {}
  LogReaderContainer(Env* env, std::shared_ptr<Logger> info_log,
                     std::string fname,
                     std::unique_ptr<SequentialFileReader>&& file_reader,
                     uint64_t log_number) {
    LogReporter* reporter = new LogReporter();
    status_ = new Status();
    reporter->env = env;
    reporter->info_log = info_log.get();
    reporter->fname = std::move(fname);
    reporter->status = status_;
    reporter_ = reporter;
    // We intentially make log::Reader do checksumming even if
    // paranoid_checks==false so that corruptions cause entire commits
    // to be skipped instead of propagating bad information (like overly
    // large sequence numbers).
    reader_ = new log::FragmentBufferedReader(info_log, std::move(file_reader),
                                              reporter, true /*checksum*/,
                                              log_number);
  }
  log::FragmentBufferedReader* reader_;
  log::Reader::Reporter* reporter_;
  Status* status_;
  ~LogReaderContainer() {
    delete reader_;
    delete reporter_;
    delete status_;
  }
 private:
  struct LogReporter : public log::Reader::Reporter {
    Env* env;
    Logger* info_log;
    std::string fname;
    Status* status;  // nullptr if immutable_db_options_.paranoid_checks==false
    void Corruption(size_t bytes, const Status& s) override {
      ROCKS_LOG_WARN(info_log, "%s%s: dropping %d bytes; %s",
                     (this->status == nullptr ? "(ignoring error) " : ""),
                     fname.c_str(), static_cast<int>(bytes),
                     s.ToString().c_str());
      if (this->status != nullptr && this->status->ok()) {
        *this->status = s;
      }
    }
  };
};

class DBImplSecondary : public DBImpl {
 public:
  DBImplSecondary(const DBOptions& options, const std::string& dbname);
  ~DBImplSecondary() override;

  Status Recover(const std::vector<ColumnFamilyDescriptor>& column_families,
                 bool read_only, bool error_if_log_file_exist,
                 bool error_if_data_exists_in_logs) override;

  // Implementations of the DB interface
  using DB::Get;
  Status Get(const ReadOptions& options, ColumnFamilyHandle* column_family,
             const Slice& key, PinnableSlice* value) override;

  Status GetImpl(const ReadOptions& options, ColumnFamilyHandle* column_family,
                 const Slice& key, PinnableSlice* value);

  using DBImpl::NewIterator;
  Iterator* NewIterator(const ReadOptions&,
                        ColumnFamilyHandle* column_family) override;

  ArenaWrappedDBIter* NewIteratorImpl(const ReadOptions& read_options,
                                      ColumnFamilyData* cfd,
                                      SequenceNumber snapshot,
                                      ReadCallback* read_callback);

  Status NewIterators(const ReadOptions& options,
                      const std::vector<ColumnFamilyHandle*>& column_families,
                      std::vector<Iterator*>* iterators) override;

  using DBImpl::Put;
  Status Put(const WriteOptions& /*options*/,
             ColumnFamilyHandle* /*column_family*/, const Slice& /*key*/,
             const Slice& /*value*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::Merge;
  Status Merge(const WriteOptions& /*options*/,
               ColumnFamilyHandle* /*column_family*/, const Slice& /*key*/,
               const Slice& /*value*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::Delete;
  Status Delete(const WriteOptions& /*options*/,
                ColumnFamilyHandle* /*column_family*/,
                const Slice& /*key*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::SingleDelete;
  Status SingleDelete(const WriteOptions& /*options*/,
                      ColumnFamilyHandle* /*column_family*/,
                      const Slice& /*key*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  Status Write(const WriteOptions& /*options*/,
               WriteBatch* /*updates*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::CompactRange;
  Status CompactRange(const CompactRangeOptions& /*options*/,
                      ColumnFamilyHandle* /*column_family*/,
                      const Slice* /*begin*/, const Slice* /*end*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::CompactFiles;
  Status CompactFiles(
      const CompactionOptions& /*compact_options*/,
      ColumnFamilyHandle* /*column_family*/,
      const std::vector<std::string>& /*input_file_names*/,
      const int /*output_level*/, const int /*output_path_id*/ = -1,
      std::vector<std::string>* const /*output_file_names*/ = nullptr,
      CompactionJobInfo* /*compaction_job_info*/ = nullptr) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  Status DisableFileDeletions() override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  Status EnableFileDeletions(bool /*force*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  Status GetLiveFiles(std::vector<std::string>&,
                      uint64_t* /*manifest_file_size*/,
                      bool /*flush_memtable*/ = true) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::Flush;
  Status Flush(const FlushOptions& /*options*/,
               ColumnFamilyHandle* /*column_family*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DBImpl::SyncWAL;
  Status SyncWAL() override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  using DB::IngestExternalFile;
  Status IngestExternalFile(
      ColumnFamilyHandle* /*column_family*/,
      const std::vector<std::string>& /*external_files*/,
      const IngestExternalFileOptions& /*ingestion_options*/) override {
    return Status::NotSupported("Not supported operation in secondary mode.");
  }

  // Try to catch up with the primary by reading as much as possible from the
  // log files until there is nothing more to read or encounters an error. If
  // the amount of information in the log files to process is huge, this
  // method can take long time due to all the I/O and CPU costs.
  Status TryCatchUpWithPrimary() override;

  Status MaybeInitLogReader(uint64_t log_number,
                            log::FragmentBufferedReader** log_reader);

 protected:
  class ColumnFamilyCollector : public WriteBatch::Handler {
    std::unordered_set<uint32_t> column_family_ids_;

    Status AddColumnFamilyId(uint32_t column_family_id) {
      if (column_family_ids_.find(column_family_id) ==
          column_family_ids_.end()) {
        column_family_ids_.insert(column_family_id);
      }
      return Status::OK();
    }

   public:
    explicit ColumnFamilyCollector() {}

    ~ColumnFamilyCollector() override {}

    Status PutCF(uint32_t column_family_id, const Slice&,
                 const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    Status DeleteCF(uint32_t column_family_id, const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    Status SingleDeleteCF(uint32_t column_family_id, const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    Status DeleteRangeCF(uint32_t column_family_id, const Slice&,
                         const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    Status MergeCF(uint32_t column_family_id, const Slice&,
                   const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    Status PutBlobIndexCF(uint32_t column_family_id, const Slice&,
                          const Slice&) override {
      return AddColumnFamilyId(column_family_id);
    }

    const std::unordered_set<uint32_t>& column_families() const {
      return column_family_ids_;
    }
  };

  Status CollectColumnFamilyIdsFromWriteBatch(
      const WriteBatch& batch, std::vector<uint32_t>* column_family_ids) {
    assert(column_family_ids != nullptr);
    column_family_ids->clear();
    ColumnFamilyCollector handler;
    Status s = batch.Iterate(&handler);
    if (s.ok()) {
      for (const auto& cf : handler.column_families()) {
        column_family_ids->push_back(cf);
      }
    }
    return s;
  }

 private:
  friend class DB;

  // No copying allowed
  DBImplSecondary(const DBImplSecondary&);
  void operator=(const DBImplSecondary&);

  using DBImpl::Recover;

  Status FindAndRecoverLogFiles(
      std::unordered_set<ColumnFamilyData*>* cfds_changed,
      JobContext* job_context);
  Status FindNewLogNumbers(std::vector<uint64_t>* logs);
  Status RecoverLogFiles(const std::vector<uint64_t>& log_numbers,
                         SequenceNumber* next_sequence,
                         std::unordered_set<ColumnFamilyData*>* cfds_changed,
                         JobContext* job_context);

  std::unique_ptr<log::FragmentBufferedReader> manifest_reader_;
  std::unique_ptr<log::Reader::Reporter> manifest_reporter_;
  std::unique_ptr<Status> manifest_reader_status_;

  // Cache log readers for each log number, used for continue WAL replay
  // after recovery
  std::map<uint64_t, std::unique_ptr<LogReaderContainer>> log_readers_;

  // Current WAL number replayed for each column family.
  std::unordered_map<ColumnFamilyData*, uint64_t> cfd_to_current_log_;
};

}  // namespace rocksdb

#endif  // !ROCKSDB_LITE
