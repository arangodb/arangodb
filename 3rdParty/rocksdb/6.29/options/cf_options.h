// Copyright (c) 2011-present, Facebook, Inc.  All rights reserved.
//  This source code is licensed under both the GPLv2 (found in the
//  COPYING file in the root directory) and Apache 2.0 License
//  (found in the LICENSE.Apache file in the root directory).

#pragma once

#include <string>
#include <vector>

#include "db/dbformat.h"
#include "options/db_options.h"
#include "rocksdb/options.h"
#include "util/compression.h"

namespace ROCKSDB_NAMESPACE {

// ImmutableCFOptions is a data struct used by RocksDB internal. It contains a
// subset of Options that should not be changed during the entire lifetime
// of DB. Raw pointers defined in this struct do not have ownership to the data
// they point to. Options contains std::shared_ptr to these data.
struct ImmutableCFOptions {
 public:
  static const char* kName() { return "ImmutableCFOptions"; }
  explicit ImmutableCFOptions();
  explicit ImmutableCFOptions(const ColumnFamilyOptions& cf_options);

  CompactionStyle compaction_style;

  CompactionPri compaction_pri;

  const Comparator* user_comparator;
  InternalKeyComparator internal_comparator;  // Only in Immutable

  std::shared_ptr<MergeOperator> merge_operator;

  const CompactionFilter* compaction_filter;

  std::shared_ptr<CompactionFilterFactory> compaction_filter_factory;

  int min_write_buffer_number_to_merge;

  int max_write_buffer_number_to_maintain;

  int64_t max_write_buffer_size_to_maintain;

  bool inplace_update_support;

  UpdateStatus (*inplace_callback)(char* existing_value,
                                   uint32_t* existing_value_size,
                                   Slice delta_value,
                                   std::string* merged_value);

  std::shared_ptr<MemTableRepFactory> memtable_factory;

  std::shared_ptr<TableFactory> table_factory;

  Options::TablePropertiesCollectorFactories
      table_properties_collector_factories;

  // This options is required by PlainTableReader. May need to move it
  // to PlainTableOptions just like bloom_bits_per_key
  uint32_t bloom_locality;

  bool purge_redundant_kvs_while_flush;

  std::vector<CompressionType> compression_per_level;

  bool level_compaction_dynamic_level_bytes;

  int num_levels;

  bool optimize_filters_for_hits;

  bool force_consistency_checks;

  std::shared_ptr<const SliceTransform>
      memtable_insert_with_hint_prefix_extractor;

  std::vector<DbPath> cf_paths;

  std::shared_ptr<ConcurrentTaskLimiter> compaction_thread_limiter;

  std::shared_ptr<SstPartitionerFactory> sst_partitioner_factory;
};

struct ImmutableOptions : public ImmutableDBOptions, public ImmutableCFOptions {
  explicit ImmutableOptions();
  explicit ImmutableOptions(const Options& options);

  ImmutableOptions(const DBOptions& db_options,
                   const ColumnFamilyOptions& cf_options);

  ImmutableOptions(const ImmutableDBOptions& db_options,
                   const ImmutableCFOptions& cf_options);

  ImmutableOptions(const DBOptions& db_options,
                   const ImmutableCFOptions& cf_options);

  ImmutableOptions(const ImmutableDBOptions& db_options,
                   const ColumnFamilyOptions& cf_options);
};

struct MutableCFOptions {
  static const char* kName() { return "MutableCFOptions"; }
  explicit MutableCFOptions(const ColumnFamilyOptions& options)
      : write_buffer_size(options.write_buffer_size),
        max_write_buffer_number(options.max_write_buffer_number),
        arena_block_size(options.arena_block_size),
        memtable_prefix_bloom_size_ratio(
            options.memtable_prefix_bloom_size_ratio),
        memtable_whole_key_filtering(options.memtable_whole_key_filtering),
        memtable_huge_page_size(options.memtable_huge_page_size),
        max_successive_merges(options.max_successive_merges),
        inplace_update_num_locks(options.inplace_update_num_locks),
        prefix_extractor(options.prefix_extractor),
        disable_auto_compactions(options.disable_auto_compactions),
        soft_pending_compaction_bytes_limit(
            options.soft_pending_compaction_bytes_limit),
        hard_pending_compaction_bytes_limit(
            options.hard_pending_compaction_bytes_limit),
        level0_file_num_compaction_trigger(
            options.level0_file_num_compaction_trigger),
        level0_slowdown_writes_trigger(options.level0_slowdown_writes_trigger),
        level0_stop_writes_trigger(options.level0_stop_writes_trigger),
        max_compaction_bytes(options.max_compaction_bytes),
        target_file_size_base(options.target_file_size_base),
        target_file_size_multiplier(options.target_file_size_multiplier),
        max_bytes_for_level_base(options.max_bytes_for_level_base),
        max_bytes_for_level_multiplier(options.max_bytes_for_level_multiplier),
        ttl(options.ttl),
        periodic_compaction_seconds(options.periodic_compaction_seconds),
        max_bytes_for_level_multiplier_additional(
            options.max_bytes_for_level_multiplier_additional),
        compaction_options_fifo(options.compaction_options_fifo),
        compaction_options_universal(options.compaction_options_universal),
        enable_blob_files(options.enable_blob_files),
        min_blob_size(options.min_blob_size),
        blob_file_size(options.blob_file_size),
        blob_compression_type(options.blob_compression_type),
        enable_blob_garbage_collection(options.enable_blob_garbage_collection),
        blob_garbage_collection_age_cutoff(
            options.blob_garbage_collection_age_cutoff),
        blob_garbage_collection_force_threshold(
            options.blob_garbage_collection_force_threshold),
        blob_compaction_readahead_size(options.blob_compaction_readahead_size),
        max_sequential_skip_in_iterations(
            options.max_sequential_skip_in_iterations),
        check_flush_compaction_key_order(
            options.check_flush_compaction_key_order),
        paranoid_file_checks(options.paranoid_file_checks),
        report_bg_io_stats(options.report_bg_io_stats),
        compression(options.compression),
        bottommost_compression(options.bottommost_compression),
        compression_opts(options.compression_opts),
        bottommost_compression_opts(options.bottommost_compression_opts),
        bottommost_temperature(options.bottommost_temperature),
        sample_for_compression(
            options.sample_for_compression) {  // TODO: is 0 fine here?
    RefreshDerivedOptions(options.num_levels, options.compaction_style);
  }

  MutableCFOptions()
      : write_buffer_size(0),
        max_write_buffer_number(0),
        arena_block_size(0),
        memtable_prefix_bloom_size_ratio(0),
        memtable_whole_key_filtering(false),
        memtable_huge_page_size(0),
        max_successive_merges(0),
        inplace_update_num_locks(0),
        prefix_extractor(nullptr),
        disable_auto_compactions(false),
        soft_pending_compaction_bytes_limit(0),
        hard_pending_compaction_bytes_limit(0),
        level0_file_num_compaction_trigger(0),
        level0_slowdown_writes_trigger(0),
        level0_stop_writes_trigger(0),
        max_compaction_bytes(0),
        target_file_size_base(0),
        target_file_size_multiplier(0),
        max_bytes_for_level_base(0),
        max_bytes_for_level_multiplier(0),
        ttl(0),
        periodic_compaction_seconds(0),
        compaction_options_fifo(),
        enable_blob_files(false),
        min_blob_size(0),
        blob_file_size(0),
        blob_compression_type(kNoCompression),
        enable_blob_garbage_collection(false),
        blob_garbage_collection_age_cutoff(0.0),
        blob_garbage_collection_force_threshold(0.0),
        blob_compaction_readahead_size(0),
        max_sequential_skip_in_iterations(0),
        check_flush_compaction_key_order(true),
        paranoid_file_checks(false),
        report_bg_io_stats(false),
        compression(Snappy_Supported() ? kSnappyCompression : kNoCompression),
        bottommost_compression(kDisableCompressionOption),
        bottommost_temperature(Temperature::kUnknown),
        sample_for_compression(0) {}

  explicit MutableCFOptions(const Options& options);

  // Must be called after any change to MutableCFOptions
  void RefreshDerivedOptions(int num_levels, CompactionStyle compaction_style);

  void RefreshDerivedOptions(const ImmutableCFOptions& ioptions) {
    RefreshDerivedOptions(ioptions.num_levels, ioptions.compaction_style);
  }

  int MaxBytesMultiplerAdditional(int level) const {
    if (level >=
        static_cast<int>(max_bytes_for_level_multiplier_additional.size())) {
      return 1;
    }
    return max_bytes_for_level_multiplier_additional[level];
  }

  void Dump(Logger* log) const;

  // Memtable related options
  size_t write_buffer_size;
  int max_write_buffer_number;
  size_t arena_block_size;
  double memtable_prefix_bloom_size_ratio;
  bool memtable_whole_key_filtering;
  size_t memtable_huge_page_size;
  size_t max_successive_merges;
  size_t inplace_update_num_locks;
  std::shared_ptr<const SliceTransform> prefix_extractor;

  // Compaction related options
  bool disable_auto_compactions;
  uint64_t soft_pending_compaction_bytes_limit;
  uint64_t hard_pending_compaction_bytes_limit;
  int level0_file_num_compaction_trigger;
  int level0_slowdown_writes_trigger;
  int level0_stop_writes_trigger;
  uint64_t max_compaction_bytes;
  uint64_t target_file_size_base;
  int target_file_size_multiplier;
  uint64_t max_bytes_for_level_base;
  double max_bytes_for_level_multiplier;
  uint64_t ttl;
  uint64_t periodic_compaction_seconds;
  std::vector<int> max_bytes_for_level_multiplier_additional;
  CompactionOptionsFIFO compaction_options_fifo;
  CompactionOptionsUniversal compaction_options_universal;

  // Blob file related options
  bool enable_blob_files;
  uint64_t min_blob_size;
  uint64_t blob_file_size;
  CompressionType blob_compression_type;
  bool enable_blob_garbage_collection;
  double blob_garbage_collection_age_cutoff;
  double blob_garbage_collection_force_threshold;
  uint64_t blob_compaction_readahead_size;

  // Misc options
  uint64_t max_sequential_skip_in_iterations;
  bool check_flush_compaction_key_order;
  bool paranoid_file_checks;
  bool report_bg_io_stats;
  CompressionType compression;
  CompressionType bottommost_compression;
  CompressionOptions compression_opts;
  CompressionOptions bottommost_compression_opts;
  // TODO this experimental option isn't made configurable
  // through strings yet.
  Temperature bottommost_temperature;

  uint64_t sample_for_compression;

  // Derived options
  // Per-level target file size.
  std::vector<uint64_t> max_file_size;
};

uint64_t MultiplyCheckOverflow(uint64_t op1, double op2);

// Get the max file size in a given level.
uint64_t MaxFileSizeForLevel(const MutableCFOptions& cf_options,
    int level, CompactionStyle compaction_style, int base_level = 1,
    bool level_compaction_dynamic_level_bytes = false);

// Get the max size of an L0 file for which we will pin its meta-blocks when
// `pin_l0_filter_and_index_blocks_in_cache` is set.
size_t MaxFileSizeForL0MetaPin(const MutableCFOptions& cf_options);

#ifndef ROCKSDB_LITE
Status GetStringFromMutableCFOptions(const ConfigOptions& config_options,
                                     const MutableCFOptions& mutable_opts,
                                     std::string* opt_string);

Status GetMutableOptionsFromStrings(
    const MutableCFOptions& base_options,
    const std::unordered_map<std::string, std::string>& options_map,
    Logger* info_log, MutableCFOptions* new_options);
#endif  // ROCKSDB_LITE

}  // namespace ROCKSDB_NAMESPACE
