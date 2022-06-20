////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBOptions.h"

#include "Basics/PhysicalMemory.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

#include <rocksdb/filter_policy.h>

namespace {
rocksdb::TransactionDBOptions rocksDBTrxDefaults;
rocksdb::Options rocksDBDefaults;
rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;

uint64_t defaultBlockCacheSize() {
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.3
    return static_cast<uint64_t>((arangodb::PhysicalMemory::getValue() -
                                  (static_cast<uint64_t>(2) << 30)) *
                                 0.3);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(2) << 30)) {
    // if we have at least 2GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 256MB
    return (static_cast<uint64_t>(256) << 20);
  }
  // for everything else the default size is 128MB
  return (static_cast<uint64_t>(128) << 20);
}

uint64_t defaultTotalWriteBufferSize() {
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.4
    return static_cast<uint64_t>((arangodb::PhysicalMemory::getValue() -
                                  (static_cast<uint64_t>(2) << 30)) *
                                 0.4);
  }
  if (arangodb::PhysicalMemory::getValue() >=
      (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  // for everything else the default size is 256MB
  return (static_cast<uint64_t>(256) << 20);
}

uint64_t defaultMinWriteBufferNumberToMerge(uint64_t totalSize,
                                            uint64_t sizePerBuffer,
                                            uint64_t maxBuffers) {
  uint64_t safe = rocksDBDefaults.min_write_buffer_number_to_merge;
  uint64_t test = safe + 1;

  // increase it to as much as 4 if it makes sense
  for (; test <= 4; ++test) {
    // next make sure we have enough buffers for it to matter
    uint64_t minBuffers = 1 + (2 * test);
    if (maxBuffers < minBuffers) {
      break;
    }

    // next make sure we have enough space for all the buffers
    if (minBuffers * sizePerBuffer *
            arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies >
        totalSize) {
      break;
    }
    safe = test;
  }
  return safe;
}

}  // namespace

namespace arangodb::sepp {

RocksDBOptions::TableOptions::LruCacheOptions::LruCacheOptions()
    : blockCacheSize(::defaultBlockCacheSize()),
      blockCacheShardBits(-1),
      enforceBlockCacheSizeLimit(true) {}

RocksDBOptions::RocksDBOptions()
    : _dbOptions{.numStripes = std::thread::hardware_concurrency(),
                 .transactionLockTimeout =
                     rocksDBTrxDefaults.transaction_lock_timeout},
      _tableOptions{
          .blockCache = TableOptions::LruCacheOptions{},
          .cacheIndexAndFilterBlocks = true,
          .cacheIndexAndFilterBlocksWithHighPriority =
              rocksDBTableOptionsDefaults
                  .cache_index_and_filter_blocks_with_high_priority,
          .pinl0FilterAndIndexBlocksInCache =
              rocksDBTableOptionsDefaults
                  .pin_l0_filter_and_index_blocks_in_cache,
          .pinTopLevelIndexAndFilter =
              rocksDBTableOptionsDefaults.pin_top_level_index_and_filter,
          .enableIndexCompression =
              rocksDBTableOptionsDefaults.enable_index_compression,
          .prepopulateBlockCache =
              rocksDBTableOptionsDefaults.prepopulate_block_cache ==
              rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::
                  kFlushOnly,
          .reserveTableBuilderMemory =
              rocksDBTableOptionsDefaults.reserve_table_builder_memory,
          .reserveTableReaderMemory =
              rocksDBTableOptionsDefaults.reserve_table_reader_memory,

          .blockSize = std::max(
              rocksDBTableOptionsDefaults.block_size,
              static_cast<decltype(rocksDBTableOptionsDefaults.block_size)>(
                  16 * 1024)),

          .filterPolicy =
              TableOptions::BloomFilterPolicy{.bitsPerKey = 10,
                                              .useBlockBasedBuilder = true},
          .formatVersion = 5,
          .blockAlignDataBlocks = rocksDBTableOptionsDefaults.block_align,
          .checksum = "crc32c",         // TODO - use enum
          .compressionType = "snappy",  // TODO - use enum
      },
      _options{
          .numThreadsLow = 1,
          .numThreadsHigh = 1,

          .maxTotalWalSize = 80 << 20,
          .allowFAllocate = true,

          .enablePipelinedWrite = true,
          .writeBufferSize = rocksDBDefaults.write_buffer_size,
          .maxWriteBufferNumber = 8 + 2,  // number of column families plus 2
          .maxWriteBufferNumberToMaintain = 1,
          .maxWriteBufferSizeToMaintain = 0,
          .delayedWriteRate = rocksDBDefaults.delayed_write_rate,
          .minWriteBufferNumberToMerge = defaultMinWriteBufferNumberToMerge(
              rocksDBDefaults.db_write_buffer_size,
              rocksDBDefaults.write_buffer_size, 8 + 2),
          .numLevels = static_cast<uint64_t>(rocksDBDefaults.num_levels),
          .levelCompactionDynamicLevelBytes = true,
          .maxBytesForLevelBase = rocksDBDefaults.max_bytes_for_level_base,
          .maxBytesForLevelMultiplier =
              rocksDBDefaults.max_bytes_for_level_multiplier,
          .optimizeFiltersForHits = rocksDBDefaults.optimize_filters_for_hits,
          .useDirectReads = rocksDBDefaults.use_direct_reads,
          .useDirectIoForFlushAndCompaction =
              rocksDBDefaults.use_direct_io_for_flush_and_compaction,

          .targetFileSizeBase = rocksDBDefaults.target_file_size_base,
          .targetFileSizeMultiplier = static_cast<uint64_t>(
              rocksDBDefaults.target_file_size_multiplier),

          .maxBackgroundJobs = rocksDBDefaults.max_background_jobs,
          .maxSubcompactions = 2,
          .useFSync = rocksDBDefaults.use_fsync,

          .numUncompressedLevels = 2,

          // TODO - compression algo

          // Number of files to trigger level-0 compaction. A value <0 means
          // that level-0 compaction will not be triggered by number of files at
          // all. Default: 4
          .level0FileNumCompactionTrigger = 2,

          // Soft limit on number of level-0 files. We start slowing down writes
          // at this point. A value <0 means that no writing slow down will be
          // triggered by number of files in level-0.
          .level0SlowdownWritesTrigger = 16,

          // Maximum number of level-0 files.  We stop writes at this point.
          .level0StopWritesTrigger = 256,

          // Soft limit on pending compaction bytes. We start slowing down
          // writes at this point.
          .pendingCompactionBytesSlowdownTrigger = 128 * 1024ull,

          // Maximum number of pending compaction bytes. We stop writes at this
          // point.
          .pendingCompactionBytesStopTrigger = 16 * 1073741824ull,

          .recycleLogFileNum = rocksDBDefaults.recycle_log_file_num,
          .compactionReadaheadSize = 2 * 1024 * 1024,

          .enableStatistics = false,

          .totalWriteBufferSize = rocksDBDefaults.db_write_buffer_size,

          .memtablePrefixBloomSizeRatio = 0.2,
          .bloomLocality = 1,
      } {
  // setting the number of background jobs to
  _options.maxBackgroundJobs = static_cast<int32_t>(
      std::max<std::size_t>(2, std::thread::hardware_concurrency()));

  if (_options.totalWriteBufferSize == 0) {
    // unlimited write buffer size... now set to some fraction of physical RAM
    _options.totalWriteBufferSize = ::defaultTotalWriteBufferSize();
  }

  uint32_t max = _options.maxBackgroundJobs / 2;
  uint32_t clamped =
      std::max(std::min(std::thread::hardware_concurrency(), max), 1U);
  if (_options.numThreadsHigh == 0) {
    _options.numThreadsHigh = clamped;
  }
  if (_options.numThreadsLow == 0) {
    _options.numThreadsLow = clamped;
  }
}

rocksdb::TransactionDBOptions RocksDBOptions::getTransactionDBOptions() const {
  rocksdb::TransactionDBOptions result;
  // number of locks per column_family
  result.num_stripes = _dbOptions.numStripes;
  result.transaction_lock_timeout = _dbOptions.transactionLockTimeout;
  return result;
}

rocksdb::Options RocksDBOptions::doGetOptions() const {
  rocksdb::Options result;
  result.allow_fallocate = _options.allowFAllocate;
  result.enable_pipelined_write = _options.enablePipelinedWrite;
  result.write_buffer_size = _options.writeBufferSize;
  result.max_write_buffer_number = _options.maxWriteBufferNumber;
  result.max_write_buffer_number_to_maintain =
      _options.maxWriteBufferNumberToMaintain;
  result.max_write_buffer_size_to_maintain =
      _options.maxWriteBufferSizeToMaintain;
  result.delayed_write_rate = _options.delayedWriteRate;
  result.min_write_buffer_number_to_merge =
      _options.minWriteBufferNumberToMerge;
  result.num_levels = static_cast<int>(_options.numLevels);
  result.level_compaction_dynamic_level_bytes =
      _options.levelCompactionDynamicLevelBytes;
  result.max_bytes_for_level_base = _options.maxBytesForLevelBase;
  result.max_bytes_for_level_multiplier =
      static_cast<int>(_options.maxBytesForLevelMultiplier);
  result.optimize_filters_for_hits = _options.optimizeFiltersForHits;
  result.use_direct_reads = _options.useDirectReads;
  result.use_direct_io_for_flush_and_compaction =
      _options.useDirectIoForFlushAndCompaction;

  result.target_file_size_base = _options.targetFileSizeBase;
  result.target_file_size_multiplier =
      static_cast<int>(_options.targetFileSizeMultiplier);

  // during startup, limit the total WAL size to a small value so we do not see
  // large WAL files created at startup.
  // Instead, we will start with a small value here and up it later in the
  // startup process
  result.max_total_wal_size = 4 * 1024 * 1024;

  result.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;

  result.max_background_jobs = static_cast<int>(_options.maxBackgroundJobs);
  result.max_subcompactions = _options.maxSubcompactions;
  result.use_fsync = _options.useFSync;

  // only compress levels >= 2
  rocksdb::CompressionType ct = [&compressionType =
                                     _tableOptions.compressionType]() {
    if (compressionType == "none") {
      return rocksdb::kNoCompression;
    } else if (compressionType == "snappy") {
      return rocksdb::kSnappyCompression;
    } else if (compressionType == "lz4") {
      return rocksdb::kLZ4Compression;
    } else if (compressionType == "lz4hc") {
      return rocksdb::kLZ4HCCompression;
    } else {
      throw std::runtime_error("Unsupported compression type " +
                               compressionType);
    }
  }();

  result.compression_per_level.resize(result.num_levels);
  for (int level = 0; level < result.num_levels; ++level) {
    result.compression_per_level[level] =
        (((uint64_t)level >= _options.numUncompressedLevels)
             ? ct
             : rocksdb::kNoCompression);
  }

  // Number of files to trigger level-0 compaction. A value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  // Default: 4
  result.level0_file_num_compaction_trigger =
      static_cast<int>(_options.level0FileNumCompactionTrigger);

  // Soft limit on number of level-0 files. We start slowing down writes at this
  // point. A value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  result.level0_slowdown_writes_trigger =
      static_cast<int>(_options.level0SlowdownWritesTrigger);

  // Maximum number of level-0 files.  We stop writes at this point.
  result.level0_stop_writes_trigger =
      static_cast<int>(_options.level0StopWritesTrigger);

  // Soft limit on pending compaction bytes. We start slowing down writes
  // at this point.
  result.soft_pending_compaction_bytes_limit =
      _options.pendingCompactionBytesSlowdownTrigger;

  // Maximum number of pending compaction bytes. We stop writes at this point.
  result.hard_pending_compaction_bytes_limit =
      _options.pendingCompactionBytesStopTrigger;

  result.recycle_log_file_num = _options.recycleLogFileNum;
  result.compaction_readahead_size =
      static_cast<size_t>(_options.compactionReadaheadSize);

  // intentionally set the RocksDB logger to ERROR because it will
  // log lots of things otherwise
  result.info_log_level = rocksdb::InfoLogLevel::ERROR_LEVEL;

  if (_options.enableStatistics) {
    result.statistics = rocksdb::CreateDBStatistics();
    // result.stats_dump_period_sec = 1;
  }

  result.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(getTableOptions()));

  result.create_if_missing = true;
  result.create_missing_column_families = true;
  result.max_open_files = -1;

  if (_options.totalWriteBufferSize > 0) {
    result.db_write_buffer_size = _options.totalWriteBufferSize;
  }

  // WAL_ttl_seconds needs to be bigger than the sync interval of the count
  // manager. Should be several times bigger counter_sync_seconds
  result.WAL_ttl_seconds = 60 * 60 * 24 * 30;
  // we manage WAL file deletion ourselves, don't let RocksDB garbage collect
  // them
  result.WAL_size_limit_MB = 0;
  result.memtable_prefix_bloom_size_ratio = 0.2;  // TODO: pick better value?
  // TODO: enable memtable_insert_with_hint_prefix_extractor?
  result.bloom_locality = 1;

  return result;
}

rocksdb::BlockBasedTableOptions RocksDBOptions::doGetTableOptions() const {
  rocksdb::BlockBasedTableOptions result;

  result.block_cache = std::visit(
      [](TableOptions::LruCacheOptions const& opts)
          -> std::shared_ptr<rocksdb::Cache> {
        if (opts.blockCacheSize > 0) {
          return rocksdb::NewLRUCache(
              opts.blockCacheSize, opts.blockCacheShardBits,
              /*strict_capacity_limit*/ opts.enforceBlockCacheSizeLimit);
        }
        return nullptr;
      },
      _tableOptions.blockCache);
  if (result.block_cache == nullptr) {
    result.no_block_cache = true;
  }

  result.cache_index_and_filter_blocks =
      _tableOptions.cacheIndexAndFilterBlocks;
  result.cache_index_and_filter_blocks_with_high_priority =
      _tableOptions.cacheIndexAndFilterBlocksWithHighPriority;
  result.pin_l0_filter_and_index_blocks_in_cache =
      _tableOptions.pinl0FilterAndIndexBlocksInCache;
  result.pin_top_level_index_and_filter =
      _tableOptions.pinTopLevelIndexAndFilter;
  result.enable_index_compression = _tableOptions.enableIndexCompression;
  result.prepopulate_block_cache =
      _tableOptions.prepopulateBlockCache
          ? rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly
          : rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kDisable;
  result.reserve_table_builder_memory = _tableOptions.reserveTableBuilderMemory;
  result.reserve_table_reader_memory = _tableOptions.reserveTableReaderMemory;

  result.block_size = _tableOptions.blockSize;
  result.filter_policy = std::visit(
      [](TableOptions::BloomFilterPolicy const& filter) {
        return std::shared_ptr<const rocksdb::FilterPolicy>(
            rocksdb::NewBloomFilterPolicy(filter.bitsPerKey,
                                          filter.useBlockBasedBuilder));
      },
      _tableOptions.filterPolicy);

  result.format_version = _tableOptions.formatVersion;
  result.block_align = _tableOptions.blockAlignDataBlocks;
  result.checksum = [&checksum = _tableOptions.checksum]() {
    if (checksum == "none") {
      return rocksdb::kNoChecksum;
    } else if (checksum == "crc32c") {
      return rocksdb::kCRC32c;
    } else if (checksum == "xxHash") {
      return rocksdb::kxxHash;
    } else if (checksum == "xxHash64") {
      return rocksdb::kxxHash64;
    } else if (checksum == "XXH3") {
      return rocksdb::kXXH3;
    } else {
      throw std::runtime_error("Unsupported checksum type " + checksum);
    }
  }();

  return result;
}

uint64_t RocksDBOptions::maxTotalWalSize() const noexcept {
  return _options.maxTotalWalSize;
}
uint32_t RocksDBOptions::numThreadsHigh() const noexcept {
  return _options.numThreadsHigh;
}
uint32_t RocksDBOptions::numThreadsLow() const noexcept {
  return _options.numThreadsLow;
}

}  // namespace arangodb::sepp
