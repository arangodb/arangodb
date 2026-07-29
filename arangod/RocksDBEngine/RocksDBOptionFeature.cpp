////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstddef>
#include <ios>
#include <memory>

#include "RocksDBOptionFeature.h"
#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AgencyFeature.h"
#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

#include <rocksdb/advanced_options.h>
#include <rocksdb/cache.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/memory_allocator.h>
#include <rocksdb/options.h>
#include <rocksdb/sst_partitioner.h>
#include <rocksdb/statistics.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

// It's not atomic because it shouldn't change after initialization.
// And initialization should happen before rocksdb initialization.
static bool ioUringEnabled = true;

// weak symbol from rocksdb
extern "C" bool RocksDbIOUringEnable() { return ioUringEnabled; }

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {

rocksdb::CompressionType compressionTypeFromString(std::string_view type) {
  if (type == kCompressionTypeNone) {
    return rocksdb::kNoCompression;
  }
  if (type == kCompressionTypeSnappy) {
    return rocksdb::kSnappyCompression;
  }
  if (type == kCompressionTypeLZ4) {
    return rocksdb::kLZ4Compression;
  }
  if (type == kCompressionTypeLZ4HC) {
    return rocksdb::kLZ4HCCompression;
  }
  TRI_ASSERT(false);
  LOG_TOPIC("edc91", FATAL, arangodb::Logger::STARTUP)
      << "unexpected compression type '" << type << "'";
  FATAL_ERROR_EXIT();
}

rocksdb::CompactionStyle compactionStyleFromString(std::string_view type) {
  if (type == kCompactionStyleLevel) {
    return rocksdb::kCompactionStyleLevel;
  }
  if (type == kCompactionStyleUniversal) {
    return rocksdb::kCompactionStyleUniversal;
  }
  if (type == kCompactionStyleFifo) {
    return rocksdb::kCompactionStyleFIFO;
  }
  if (type == kCompactionStyleNone) {
    return rocksdb::kCompactionStyleNone;
  }

  TRI_ASSERT(false);
  LOG_TOPIC("edc92", FATAL, arangodb::Logger::STARTUP)
      << "unexpected compaction style '" << type << "'";
  FATAL_ERROR_EXIT();
}

}  // namespace

RocksDBOptionFeature::RocksDBOptionFeature(
    application_features::ApplicationServer& server,
    AgencyFeature const* agencyFeature)
    : RocksDBOptionFeature(server, agencyFeature,
                           RocksDBOptionFeatureOptionsProvider{}.options()) {}

RocksDBOptionFeature::RocksDBOptionFeature(
    application_features::ApplicationServer& server,
    AgencyFeature const* agencyFeature, RocksDBOptionFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _agencyFeature(agencyFeature) {
  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
}

void RocksDBOptionFeature::prepare() {
  ioUringEnabled = _options.ioUringEnabled;

  if (_options.enableBlobFiles) {
    LOG_TOPIC("5e48f", WARN, Logger::ENGINES)
        << "using blob files is experimental and not supported for production "
           "usage";
  }

  if (_options.compactionStyle != kCompactionStyleLevel) {
    LOG_TOPIC("6db54", WARN, Logger::ENGINES)
        << "using compaction style '" << _options.compactionStyle
        << "' is experimental and not supported for production usage";
  }

  if (_options.blockCacheType == kBlockCacheTypeHyperClock) {
    LOG_TOPIC("26f64", WARN, Logger::ENGINES)
        << "using block cache type 'hyper-clock' is experimental and not "
           "supported for production usage";
  }

  if (_options.enforceBlockCacheSizeLimit && _options.blockCacheSize > 0) {
    uint64_t shardSize =
        _options.blockCacheSize / (uint64_t(1) << _options.blockCacheShardBits);
    // if we can't store a data block of the mininmum size in the block cache,
    // we may run into problems when trying to put a large data block into the
    // cache. in this case the block cache may return a Status::Incomplete()
    // or Status::MemoryLimit() error and fail the entire read.
    // warn the user about it!
    if (shardSize < kMinBlockCacheShardSize) {
      LOG_TOPIC("31d7c", WARN, Logger::ENGINES)
          << "size of RocksDB block cache shards seems to be too low. "
          << "block cache size: " << _options.blockCacheSize
          << ", shard bits: " << _options.blockCacheShardBits
          << ", shard size: " << shardSize
          << ". it is probably useful to set "
             "`--rocksdb.enforce-block-cache-size-limit` to false "
          << "to avoid incomplete cache reads.";
    }
  }
}

void RocksDBOptionFeature::start() {
  uint32_t max = _options.maxBackgroundJobs / 2;
  uint32_t clamped = std::max(
      std::min(static_cast<uint32_t>(NumberOfCores::getValue()), max), 1U);
  // lets test this out
  if (_options.numThreadsHigh == 0) {
    _options.numThreadsHigh = clamped;
  }
  if (_options.numThreadsLow == 0) {
    _options.numThreadsLow = clamped;
  }

  if (_options.maxSubcompactions > _options.numThreadsLow) {
    if (server().options()->processingResult().touched(
            "--rocksdb.max-subcompactions")) {
      LOG_TOPIC("e7c85", WARN, Logger::ENGINES)
          << "overriding value for option `--rocksdb.max-subcompactions` to "
          << _options.numThreadsLow
          << " because the specified value is greater than the number of "
             "threads for low priority operations";
    }
    _options.maxSubcompactions = _options.numThreadsLow;
  }

  LOG_TOPIC("f66e4", TRACE, Logger::ENGINES)
      << "using RocksDB options:"
      << " wal_dir: '" << _options.walDirectory << "'"
      << ", compression type: " << _options.compressionType
      << ", write_buffer_size: " << _options.writeBufferSize
      << ", total_write_buffer_size: " << _options.totalWriteBufferSize
      << ", max_write_buffer_number: " << _options.maxWriteBufferNumber
      << ", max_write_buffer_size_to_maintain: "
      << _options.maxWriteBufferSizeToMaintain
      << ", max_total_wal_size: " << _options.maxTotalWalSize
      << ", delayed_write_rate: " << _options.delayedWriteRate
      << ", min_write_buffer_number_to_merge: "
      << _options.minWriteBufferNumberToMerge
      << ", num_levels: " << _options.numLevels
      << ", num_uncompressed_levels: " << _options.numUncompressedLevels
      << ", max_bytes_for_level_base: " << _options.maxBytesForLevelBase
      << ", max_bytes_for_level_multiplier: "
      << _options.maxBytesForLevelMultiplier
      << ", max_background_jobs: " << _options.maxBackgroundJobs
      << ", max_sub_compactions: " << _options.maxSubcompactions
      << ", target_file_size_base: " << _options.targetFileSizeBase
      << ", target_file_size_multiplier: " << _options.targetFileSizeMultiplier
      << ", num_threads_high: " << _options.numThreadsHigh
      << ", num_threads_low: " << _options.numThreadsLow
      << ", block_cache_type: " << _options.blockCacheType
      << ", use_jemalloc_allocator: " << _options.useJemallocAllocator
      << ", block_cache_size: " << _options.blockCacheSize
      << ", block_cache_shard_bits: " << _options.blockCacheShardBits
      << ", block_cache_estimated_entry_charge: "
      << _options.blockCacheEstimatedEntryCharge
      << ", block_cache_strict_capacity_limit: " << std::boolalpha
      << _options.enforceBlockCacheSizeLimit
      << ", cache_index_and_filter_blocks: " << std::boolalpha
      << _options.cacheIndexAndFilterBlocks
      << ", cache_index_and_filter_blocks_with_high_priority: "
      << std::boolalpha << _options.cacheIndexAndFilterBlocksWithHighPriority
      << ", pin_l0_filter_and_index_blocks_in_cache: " << std::boolalpha
      << _options.pinl0FilterAndIndexBlocksInCache
      << ", pin_top_level_index_and_filter: " << std::boolalpha
      << _options.pinTopLevelIndexAndFilter
      << ", table_block_size: " << _options.tableBlockSize
      << ", recycle_log_file_num: " << _options.recycleLogFileNum
      << ", compaction_read_ahead_size: " << _options.compactionReadaheadSize
      << ", level0_compaction_trigger: " << _options.level0CompactionTrigger
      << ", level0_slowdown_trigger: " << _options.level0SlowdownTrigger
      << ", periodic_compaction_ttl: " << _options.periodicCompactionTtl
      << ", checksum: " << _options.checksumType
      << ", format_version: " << _options.formatVersion
      << ", bloom_bits_per_key: " << _options.bloomBitsPerKey
      << ", enable_blob_files: " << std::boolalpha << _options.enableBlobFiles
      << ", enable_blob_cache: " << std::boolalpha << _options.enableBlobCache
      << ", min_blob_size: " << _options.minBlobSize
      << ", blob_file_size: " << _options.blobFileSize
      << ", blob_file_starting_level: " << _options.blobFileStartingLevel
      << ", blob_compression type: " << _options.blobCompressionType
      << ", enable_blob_garbage_collection: " << std::boolalpha
      << _options.enableBlobGarbageCollection
      << ", blob_garbage_collection_age_cutoff: "
      << _options.blobGarbageCollectionAgeCutoff
      << ", blob_garbage_collection_force_threshold: "
      << _options.blobGarbageCollectionForceThreshold
      << ", prepopulate_blob_cache: " << std::boolalpha
      << _options.prepopulateBlobCache
      << ", enable_index_compression: " << std::boolalpha
      << _options.enableIndexCompression
      << ", prepopulate_block_cache: " << std::boolalpha
      << _options.prepopulateBlockCache
      << ", reserve_table_builder_memory: " << std::boolalpha
      << _options.reserveTableBuilderMemory
      << ", reserve_table_reader_memory: " << std::boolalpha
      << _options.reserveTableReaderMemory
      << ", enable_pipelined_write: " << std::boolalpha
      << _options.enablePipelinedWrite
      << ", optimize_filters_for_hits: " << std::boolalpha
      << _options.optimizeFiltersForHits
      << ", use_direct_reads: " << std::boolalpha << _options.useDirectReads
      << ", use_direct_io_for_flush_and_compaction: " << std::boolalpha
      << _options.useDirectIoForFlushAndCompaction
      << ", use_fsync: " << std::boolalpha << _options.useFSync
      << ", allow_fallocate: " << std::boolalpha << _options.allowFAllocate
      << ", max_open_files limit: " << std::boolalpha
      << _options.limitOpenFilesAtStartup
      << ", dynamic_level_bytes: " << _options.dynamicLevelBytes;
}

rocksdb::TransactionDBOptions RocksDBOptionFeature::getTransactionDBOptions()
    const {
  rocksdb::TransactionDBOptions result;
  // number of locks per column_family
  result.num_stripes =
      std::max(size_t(1), static_cast<size_t>(_options.transactionLockStripes));
  result.transaction_lock_timeout = _options.transactionLockTimeout;
  return result;
}

rocksdb::Options RocksDBOptionFeature::doGetOptions() const {
  rocksdb::Options result;
  result.allow_fallocate = _options.allowFAllocate;
  result.enable_pipelined_write = _options.enablePipelinedWrite;
  result.write_buffer_size = static_cast<size_t>(_options.writeBufferSize);
  result.max_write_buffer_number =
      static_cast<int>(_options.maxWriteBufferNumber);
  // The following setting deserves an explanation: We found that if we leave
  // the default for max_write_buffer_number_to_maintain at 0, then setting
  // max_write_buffer_size_to_maintain to 0 has not the desired effect, rather
  // TransactionDB::PrepareWrap then sets the latter to -1 which in turn is
  // later corrected to max_write_buffer_number * write_buffer_size.
  // Therefore, we set the deprecated option max_write_buffer_number_to_maintain
  // to 1, so that we can then configure max_write_buffer_size_to_maintain
  // correctly. Set to -1, 0 or a concrete number as needed. The default of
  // 0 should be good, since we do not use OptimisticTransactionDBs anyway.
  result.max_write_buffer_number_to_maintain = 1;
  result.max_write_buffer_size_to_maintain =
      _options.maxWriteBufferSizeToMaintain;
  result.delayed_write_rate = _options.delayedWriteRate;
  result.min_write_buffer_number_to_merge =
      static_cast<int>(_options.minWriteBufferNumberToMerge);
  result.num_levels = static_cast<int>(_options.numLevels);
  result.level_compaction_dynamic_level_bytes = _options.dynamicLevelBytes;
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

  result.wal_dir = _options.walDirectory;

  if (_options.skipCorrupted) {
    result.wal_recovery_mode =
        rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;
  } else {
    result.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
  }

  result.max_background_jobs = static_cast<int>(_options.maxBackgroundJobs);
  result.max_subcompactions = _options.maxSubcompactions;
  result.use_fsync = _options.useFSync;

  rocksdb::CompressionType compressionType =
      ::compressionTypeFromString(_options.compressionType);

  // only compress levels >= 2
  result.compression_per_level.resize(result.num_levels);
  for (int level = 0; level < result.num_levels; ++level) {
    result.compression_per_level[level] =
        ((static_cast<uint64_t>(level) >= _options.numUncompressedLevels)
             ? compressionType
             : rocksdb::kNoCompression);
  }

  result.compaction_style =
      ::compactionStyleFromString(_options.compactionStyle);
  result.compaction_pri = rocksdb::kMinOverlappingRatio;

  // Number of files to trigger level-0 compaction. A value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  // Default: 4
  result.level0_file_num_compaction_trigger =
      static_cast<int>(_options.level0CompactionTrigger);

  // Soft limit on number of level-0 files. We start slowing down writes at this
  // point. A value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  result.level0_slowdown_writes_trigger =
      static_cast<int>(_options.level0SlowdownTrigger);

  // Maximum number of level-0 files.  We stop writes at this point.
  result.level0_stop_writes_trigger =
      static_cast<int>(_options.level0StopTrigger);

  // Soft limit on pending compaction bytes. We start slowing down writes
  // at this point.
  result.soft_pending_compaction_bytes_limit =
      _options.pendingCompactionBytesSlowdownTrigger;

  // Maximum number of pending compaction bytes. We stop writes at this point.
  result.hard_pending_compaction_bytes_limit =
      _options.pendingCompactionBytesStopTrigger;

  // table cache is only used when max_open_files != -1
  result.table_cache_numshardbits = 8;

  result.recycle_log_file_num = _options.recycleLogFileNum;
  result.compaction_readahead_size =
      static_cast<size_t>(_options.compactionReadaheadSize);

  // intentionally set the RocksDB logger to ERROR because it will
  // log lots of things otherwise
  if (!_options.useFileLogging) {
    // if we don't use file logging but log into ArangoDB's logfile,
    // we only want real errors
    result.info_log_level = rocksdb::InfoLogLevel::ERROR_LEVEL;
  }

  if (_options.enableStatistics) {
    result.statistics = rocksdb::CreateDBStatistics();
    // result.stats_dump_period_sec = 1;
  }

  result.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(getTableOptions()));

  result.create_if_missing = true;
  result.create_missing_column_families = true;

  if (_options.limitOpenFilesAtStartup) {
    result.max_open_files = 16;
    result.skip_stats_update_on_db_open = true;
    result.avoid_flush_during_recovery = true;
  } else {
    result.max_open_files = -1;
  }

  if (_options.totalWriteBufferSize > 0) {
    result.db_write_buffer_size = _options.totalWriteBufferSize;
  }

  // we manage WAL file deletion ourselves, don't let RocksDB garbage-collect
  // obsolete files.
  result.WAL_ttl_seconds =
      933120000;  // ~30 years (60 * 60 * 24 * 30 * 12 * 30)
  result.WAL_size_limit_MB = 0;
  result.memtable_prefix_bloom_size_ratio = 0.2;  // TODO: pick better value?
  // TODO: enable memtable_insert_with_hint_prefix_extractor?
  result.bloom_locality = 1;

  if (!server().options()->processingResult().touched(
          "rocksdb.max-write-buffer-number")) {
    // TODO It is unclear if this value makes sense as a default, but we aren't
    // changing it yet, in order to maintain backwards compatibility.

    // user hasn't explicitly set the number of write buffers, so we use a
    // default value based on the number of column families this is
    // cfFamilies.size() + 2 ... but _option needs to be set before
    //  building cfFamilies
    // Update max_write_buffer_number above if you change number of families
    // used
    result.max_write_buffer_number = 8 + 2;
  } else if (result.max_write_buffer_number < 4) {
    // user set the value explicitly, and it is lower than recommended
    result.max_write_buffer_number = 4;
    LOG_TOPIC("d5c49", WARN, Logger::ENGINES)
        << "overriding value for option `--rocksdb.max-write-buffer-number` "
           "to 4 because it is lower than recommended";
  }

  return result;
}

rocksdb::BlockBasedTableOptions RocksDBOptionFeature::doGetTableOptions()
    const {
  rocksdb::BlockBasedTableOptions result;

  if (_options.blockCacheSize > 0) {
    std::shared_ptr<rocksdb::MemoryAllocator> allocator;

#ifdef ARANGODB_HAVE_JEMALLOC
    if (_options.useJemallocAllocator) {
      rocksdb::JemallocAllocatorOptions jopts;
      rocksdb::Status s =
          rocksdb::NewJemallocNodumpAllocator(jopts, &allocator);
      if (!s.ok()) {
        LOG_TOPIC("004e6", FATAL, Logger::STARTUP)
            << "unable to use jemalloc allocator for RocksDB: " << s.ToString();
        FATAL_ERROR_EXIT();
      }
    }
#endif

    if (_options.blockCacheType == kBlockCacheTypeLRU) {
      rocksdb::LRUCacheOptions opts;

      opts.capacity = _options.blockCacheSize;
      opts.num_shard_bits = static_cast<int>(_options.blockCacheShardBits);
      opts.strict_capacity_limit = _options.enforceBlockCacheSizeLimit;
      opts.memory_allocator = allocator;

      result.block_cache = rocksdb::NewLRUCache(opts);
    } else if (_options.blockCacheType == kBlockCacheTypeHyperClock) {
      rocksdb::HyperClockCacheOptions opts(
          _options.blockCacheSize, _options.blockCacheEstimatedEntryCharge,
          static_cast<int>(_options.blockCacheShardBits),
          _options.enforceBlockCacheSizeLimit, allocator);

      result.block_cache = opts.MakeSharedCache();
    } else {
      TRI_ASSERT(false);
    }

  } else {
    result.no_block_cache = true;
  }

  result.cache_index_and_filter_blocks = _options.cacheIndexAndFilterBlocks;
  result.cache_index_and_filter_blocks_with_high_priority =
      _options.cacheIndexAndFilterBlocksWithHighPriority;
  result.pin_l0_filter_and_index_blocks_in_cache =
      _options.pinl0FilterAndIndexBlocksInCache;
  result.pin_top_level_index_and_filter = _options.pinTopLevelIndexAndFilter;

  result.block_size = _options.tableBlockSize;
  result.filter_policy.reset(
      rocksdb::NewBloomFilterPolicy(_options.bloomBitsPerKey, true));
  result.enable_index_compression = _options.enableIndexCompression;
  result.format_version = _options.formatVersion;
  result.optimize_filters_for_memory = _options.optimizeFiltersForMemory;
  result.prepopulate_block_cache =
      _options.prepopulateBlockCache
          ? rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly
          : rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kDisable;
  result.cache_usage_options.options_overrides.insert(
      {rocksdb::CacheEntryRole::kFilterConstruction,
       {/*.charged = */ _options.reserveTableBuilderMemory
            ? rocksdb::CacheEntryRoleOptions::Decision::kEnabled
            : rocksdb::CacheEntryRoleOptions::Decision::kDisabled}});
  result.cache_usage_options.options_overrides.insert(
      {rocksdb::CacheEntryRole::kBlockBasedTableReader,
       {/*.charged = */ _options.reserveTableReaderMemory
            ? rocksdb::CacheEntryRoleOptions::Decision::kEnabled
            : rocksdb::CacheEntryRoleOptions::Decision::kDisabled}});
  result.cache_usage_options.options_overrides.insert(
      {rocksdb::CacheEntryRole::kFileMetadata,
       {/*.charged = */ _options.reserveFileMetadataMemory
            ? rocksdb::CacheEntryRoleOptions::Decision::kEnabled
            : rocksdb::CacheEntryRoleOptions::Decision::kDisabled}});

  result.block_align = _options.blockAlignDataBlocks;

  if (_options.checksumType == kChecksumTypeCRC32C) {
    result.checksum = rocksdb::ChecksumType::kCRC32c;
  } else if (_options.checksumType == kChecksumTypeXXHash) {
    result.checksum = rocksdb::ChecksumType::kxxHash;
  } else if (_options.checksumType == kChecksumTypeXXHash64) {
    result.checksum = rocksdb::ChecksumType::kxxHash64;
  } else if (_options.checksumType == kChecksumTypeXXH3) {
    result.checksum = rocksdb::ChecksumType::kXXH3;
  } else {
    TRI_ASSERT(false);
    LOG_TOPIC("8d602", WARN, arangodb::Logger::STARTUP)
        << "unexpected value for '--rocksdb.checksum-type'";
  }

  return result;
}

rocksdb::ColumnFamilyOptions RocksDBOptionFeature::getColumnFamilyOptions(
    RocksDBColumnFamilyManager::Family family) const {
  rocksdb::ColumnFamilyOptions result =
      RocksDBOptionsProvider::getColumnFamilyOptions(family);

  if (family == RocksDBColumnFamilyManager::Family::Documents) {
    result.enable_blob_files = _options.enableBlobFiles;
    result.min_blob_size = _options.minBlobSize;
    result.blob_file_size = _options.blobFileSize;
    result.blob_compression_type =
        ::compressionTypeFromString(_options.blobCompressionType);
    result.enable_blob_garbage_collection =
        _options.enableBlobGarbageCollection;
    result.blob_garbage_collection_age_cutoff =
        _options.blobGarbageCollectionAgeCutoff;
    result.blob_garbage_collection_force_threshold =
        _options.blobGarbageCollectionForceThreshold;
    result.blob_file_starting_level = _options.blobFileStartingLevel;
    result.prepopulate_blob_cache =
        _options.prepopulateBlobCache
            ? rocksdb::PrepopulateBlobCache::kFlushOnly
            : rocksdb::PrepopulateBlobCache::kDisable;
    if (_options.enableBlobCache) {
      // use whatever block cache we use for blobs as well
      result.blob_cache = getTableOptions().block_cache;
    }
    if (_options.partitionFilesForDocumentsCf) {
      // partition .sst files by object id prefix
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
  }

  if (family == RocksDBColumnFamilyManager::Family::PrimaryIndex) {
    // partition .sst files by object id prefix
    if (_options.partitionFilesForPrimaryIndexCf) {
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
    // keep immutable mem tables around in memory for conflict checking
    result.max_write_buffer_size_to_maintain = 64 << 20;
  }

  if (family == RocksDBColumnFamilyManager::Family::EdgeIndex) {
    // partition .sst files by object id prefix
    if (_options.partitionFilesForEdgeIndexCf) {
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
  }

  if (family == RocksDBColumnFamilyManager::Family::VPackIndex) {
    // partition .sst files by object id prefix
    if (_options.partitionFilesForVPackIndexCf) {
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
  }

  if (family == RocksDBColumnFamilyManager::Family::MdiIndex ||
      family == RocksDBColumnFamilyManager::Family::MdiVPackIndex) {
    // partition .sst files by object id prefix
    if (_options.partitionFilesForMdiIndexCf) {
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
  }
  if (family == RocksDBColumnFamilyManager::Family::VectorIndex) {
    // partition .sst files by object id prefix
    if (_options.partitionFilesForVectorIndexCf) {
      result.sst_partitioner_factory =
          rocksdb::NewSstPartitionerFixedPrefixFactory(sizeof(uint64_t));
    }
  }

  // override
  std::size_t index = static_cast<
      std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(family);
  TRI_ASSERT(index < _options.maxWriteBufferNumberCf.size());
  if (_options.maxWriteBufferNumberCf[index] > 0) {
    result.max_write_buffer_number =
        static_cast<int>(_options.maxWriteBufferNumberCf[index]);
  }
  if (!_options.minWriteBufferNumberToMergeTouched) {
    rocksdb::Options rocksDBDefaults;
    result.min_write_buffer_number_to_merge =
        static_cast<int>(defaultMinWriteBufferNumberToMerge(
            rocksDBDefaults.min_write_buffer_number_to_merge,
            _options.totalWriteBufferSize, _options.writeBufferSize,
            result.max_write_buffer_number));
  }

  return result;
}
