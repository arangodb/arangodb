////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include <stddef.h>
#include <algorithm>
#include <ios>

#include "RocksDBOptionFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AgencyFeature.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Basics/process-utils.h"
#include "Basics/system-functions.h"
#include "FeaturePhases/BasicFeaturePhaseServer.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"

#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
rocksdb::TransactionDBOptions rocksDBTrxDefaults;
rocksdb::Options rocksDBDefaults;
rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;

uint64_t defaultBlockCacheSize() {
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.3 
    return static_cast<uint64_t>((PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.3);
  }
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(2) << 30)) {
    // if we have at least 2GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 256MB
    return (static_cast<uint64_t>(256) << 20);
  }
  // for everything else the default size is 128MB
  return (static_cast<uint64_t>(128) << 20);
}

uint64_t defaultTotalWriteBufferSize() {
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.4 
    return static_cast<uint64_t>((PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.4);
  } 
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(1) << 30)) {
    // if we have at least 1GB of RAM, the default size is 512MB
    return (static_cast<uint64_t>(512) << 20);
  }
  // for everything else the default size is 256MB
  return (static_cast<uint64_t>(256) << 20);
}

uint64_t defaultMinWriteBufferNumberToMerge(uint64_t totalSize, uint64_t sizePerBuffer, uint64_t maxBuffers) {
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
    if (minBuffers * sizePerBuffer * RocksDBColumnFamilyManager::numberOfColumnFamilies > totalSize) {
      break;
    }

    safe = test;
  }

  return safe;
}

}  // namespace

RocksDBOptionFeature::RocksDBOptionFeature(application_features::ApplicationServer& server)
    : application_features::ApplicationFeature(server, "RocksDBOption"),
      _transactionLockTimeout(rocksDBTrxDefaults.transaction_lock_timeout),
      _totalWriteBufferSize(rocksDBDefaults.db_write_buffer_size),
      _writeBufferSize(rocksDBDefaults.write_buffer_size),
      _maxWriteBufferNumber(7 + 2),  // number of column families plus 2
      _maxWriteBufferSizeToMaintain(0),
      _maxTotalWalSize(80 << 20),
      _delayedWriteRate(rocksDBDefaults.delayed_write_rate),
      _minWriteBufferNumberToMerge(
          defaultMinWriteBufferNumberToMerge(_totalWriteBufferSize, _writeBufferSize,
                                             _maxWriteBufferNumber)),
      _numLevels(rocksDBDefaults.num_levels),
      _numUncompressedLevels(2),
      _maxBytesForLevelBase(rocksDBDefaults.max_bytes_for_level_base),
      _maxBytesForLevelMultiplier(rocksDBDefaults.max_bytes_for_level_multiplier),
      _maxBackgroundJobs(rocksDBDefaults.max_background_jobs),
      _maxSubcompactions(rocksDBDefaults.max_subcompactions),
      _numThreadsHigh(0),
      _numThreadsLow(0),
      _targetFileSizeBase(rocksDBDefaults.target_file_size_base),
      _targetFileSizeMultiplier(rocksDBDefaults.target_file_size_multiplier),
      _blockCacheSize(::defaultBlockCacheSize()),
      _blockCacheShardBits(-1),
      _tableBlockSize(
          std::max(rocksDBTableOptionsDefaults.block_size,
                   static_cast<decltype(rocksDBTableOptionsDefaults.block_size)>(16 * 1024))),
      _compactionReadaheadSize(2 * 1024 * 1024),  // rocksDBDefaults.compaction_readahead_size
      _level0CompactionTrigger(2),
      _level0SlowdownTrigger(rocksDBDefaults.level0_slowdown_writes_trigger),
      _level0StopTrigger(rocksDBDefaults.level0_stop_writes_trigger),
      _recycleLogFileNum(rocksDBDefaults.recycle_log_file_num),
      _enforceBlockCacheSizeLimit(false),
      _cacheIndexAndFilterBlocks(rocksDBTableOptionsDefaults.cache_index_and_filter_blocks),
      _cacheIndexAndFilterBlocksWithHighPriority(
          rocksDBTableOptionsDefaults.cache_index_and_filter_blocks_with_high_priority),
      _pinl0FilterAndIndexBlocksInCache(
          rocksDBTableOptionsDefaults.pin_l0_filter_and_index_blocks_in_cache),
      _pinTopLevelIndexAndFilter(rocksDBTableOptionsDefaults.pin_top_level_index_and_filter),
      _blockAlignDataBlocks(rocksDBTableOptionsDefaults.block_align),
      _enablePipelinedWrite(rocksDBDefaults.enable_pipelined_write),
      _optimizeFiltersForHits(rocksDBDefaults.optimize_filters_for_hits),
      _useDirectReads(rocksDBDefaults.use_direct_reads),
      _useDirectIoForFlushAndCompaction(rocksDBDefaults.use_direct_io_for_flush_and_compaction),
      _useFSync(rocksDBDefaults.use_fsync),
      _skipCorrupted(false),
      _dynamicLevelBytes(true),
      _enableStatistics(false),
      _useFileLogging(false),
      _limitOpenFilesAtStartup(false),
      _allowFAllocate(true),
      _exclusiveWrites(false),
      _vpackCmp(new RocksDBVPackComparator()),
      _maxWriteBufferNumberCf{0, 0, 0, 0, 0, 0, 0},
      _minWriteBufferNumberToMergeTouched(false) {
  // setting the number of background jobs to
  _maxBackgroundJobs = static_cast<int32_t>(
      std::max(static_cast<size_t>(2), std::min(NumberOfCores::getValue(), static_cast<size_t>(8))));
#ifdef _WIN32
  // Windows code does not (yet) support lowering thread priority of
  //  compactions.  Therefore it is possible for rocksdb to use all
  //  CPU time on compactions.  Essential network communications can be lost.
  //  Save one CPU for ArangoDB network and other activities.
  if (2 < _maxBackgroundJobs) {
    --_maxBackgroundJobs;
  }  // if
#endif

  if (_totalWriteBufferSize == 0) {
    // unlimited write buffer size... now set to some fraction of physical RAM
    _totalWriteBufferSize = ::defaultTotalWriteBufferSize();
  }

  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
}

void RocksDBOptionFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");

  options->addObsoleteOption("--rocksdb.enabled",
                             "obsolete always active - Whether or not the "
                             "RocksDB engine is enabled for the persistent "
                             "index",
                             true);

  options->addOption("--rocksdb.wal-directory",
                     "optional path to the RocksDB WAL directory. "
                     "If not set, the WAL directory will be located inside the "
                     "regular data directory",
                     new StringParameter(&_walDirectory));
  
  options->addOption(
      "--rocksdb.target-file-size-base",
      "per-file target file size for compaction (in bytes). the actual target file size for each level is `--rocksdb.target-file-size-base` multiplied by `--rocksdb.target-file-size-multiplier` ^ (level - 1)",
      new UInt64Parameter(&_targetFileSizeBase))
  .setIntroducedIn(30701);
  
  options->addOption(
      "--rocksdb.target-file-size-multiplier",
      "multiplier for `--rocksdb.target-file-size`, a value of 1 means that files in different levels will have the same size",
      new UInt64Parameter(&_targetFileSizeMultiplier))
  .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.transaction-lock-timeout",
      "If positive, specifies the wait timeout in milliseconds when "
      " a transaction attempts to lock a document. A negative value "
      "is not recommended as it can lead to deadlocks (0 = no waiting, < 0 no "
      "timeout)",
      new Int64Parameter(&_transactionLockTimeout));

  options->addOption(
      "--rocksdb.total-write-buffer-size",
      "maximum total size of in-memory write buffers (0 = unbounded)",
      new UInt64Parameter(&_totalWriteBufferSize),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--rocksdb.write-buffer-size",
                     "amount of data to build up in memory before converting "
                     "to a sorted on-disk file (0 = disabled)",
                     new UInt64Parameter(&_writeBufferSize));

  options->addOption("--rocksdb.max-write-buffer-number",
                     "maximum number of write buffers that build up in memory "
                     "(default: number of column families + 2 = 9 write buffers). "
                     "You can increase the amount only",
                     new UInt64Parameter(&_maxWriteBufferNumber));

  options->addOption("--rocksdb.max-write-buffer-size-to-maintain",
                     "maximum size of immutable write buffers that build up in memory "
                     "per column family (larger values mean that more in-memory data "
                     "can be used for transaction conflict checking (-1 = use automatic default value, "
                     "0 = do not keep immutable flushed write buffers, which is the default and usually "
                     "correct))",
                     new Int64Parameter(&_maxWriteBufferSizeToMaintain))
                     .setIntroducedIn(30703);

  options->addOption("--rocksdb.max-total-wal-size",
                     "maximum total size of WAL files that will force flush "
                     "stale column families",
                     new UInt64Parameter(&_maxTotalWalSize));

  options->addOption(
      "--rocksdb.delayed-write-rate",
      "limited write rate to DB (in bytes per second) if we are writing to the "
      "last mem-table allowed and we allow more than 3 mem-tables, or if we "
      "have surpassed a certain number of level-0 files and need to slowdown "
      "writes",
      new UInt64Parameter(&_delayedWriteRate),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  options->addOldOption("rocksdb.delayed_write_rate",
                        "rocksdb.delayed-write-rate");

  options->addOption("--rocksdb.min-write-buffer-number-to-merge",
                     "minimum number of write buffers that will be merged "
                     "together before writing to storage",
                     new UInt64Parameter(&_minWriteBufferNumberToMerge),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--rocksdb.num-levels", "number of levels for the database",
                     new UInt64Parameter(&_numLevels));

  options->addOption("--rocksdb.num-uncompressed-levels",
                     "number of uncompressed levels for the database",
                     new UInt64Parameter(&_numUncompressedLevels));

  options->addOption("--rocksdb.dynamic-level-bytes",
                     "if true, determine the number of bytes for each level "
                     "dynamically to minimize space amplification",
                     new BooleanParameter(&_dynamicLevelBytes));

  options->addOption("--rocksdb.max-bytes-for-level-base",
                     "if not using dynamic level sizes, this controls the "
                     "maximum total data size for level-1",
                     new UInt64Parameter(&_maxBytesForLevelBase));

  options->addOption("--rocksdb.max-bytes-for-level-multiplier",
                     "if not using dynamic level sizes, the maximum number of "
                     "bytes for level L can be calculated as "
                     " max-bytes-for-level-base * "
                     "(max-bytes-for-level-multiplier ^ (L-1))",
                     new DoubleParameter(&_maxBytesForLevelMultiplier));

  options->addOption(
      "--rocksdb.block-align-data-blocks",
      "if true, aligns data blocks on lesser of page size and block size",
      new BooleanParameter(&_blockAlignDataBlocks));

  options->addOption(
      "--rocksdb.enable-pipelined-write",
      "if true, use a two stage write queue for WAL writes and memtable writes",
      new BooleanParameter(&_enablePipelinedWrite));

  options->addOption("--rocksdb.enable-statistics",
                     "whether or not RocksDB statistics should be turned on",
                     new BooleanParameter(&_enableStatistics));

  options->addOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for "
      "keys missed. This would be used in cases where the application knows "
      "that there are very few misses or the performance in the case of "
      "misses is not important",
      new BooleanParameter(&_optimizeFiltersForHits),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

#ifdef __linux__
  options->addOption("--rocksdb.use-direct-reads",
                     "use O_DIRECT for reading files", new BooleanParameter(&_useDirectReads),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::Hidden));

  options->addOption("--rocksdb.use-direct-io-for-flush-and-compaction",
                     "use O_DIRECT for flush and compaction",
                     new BooleanParameter(&_useDirectIoForFlushAndCompaction),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::Hidden));
#endif

  options->addOption("--rocksdb.use-fsync",
                     "issue an fsync when writing to disk (set to true "
                     "for issuing fdatasync only)",
                     new BooleanParameter(&_useFSync),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--rocksdb.max-background-jobs",
      "Maximum number of concurrent background jobs (compactions and flushes)",
      new Int32Parameter(&_maxBackgroundJobs),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                   arangodb::options::Flags::Dynamic));

  options->addOption("--rocksdb.max-subcompactions",
                     "maximum number of concurrent subjobs for a background "
                     "compaction",
                     new UInt64Parameter(&_maxSubcompactions));

  options->addOption("--rocksdb.level0-compaction-trigger",
                     "number of level-0 files that triggers a compaction",
                     new Int64Parameter(&_level0CompactionTrigger));

  options->addOption("--rocksdb.level0-slowdown-trigger",
                     "number of level-0 files that triggers a write slowdown",
                     new Int64Parameter(&_level0SlowdownTrigger));

  options->addOption("--rocksdb.level0-stop-trigger",
                     "number of level-0 files that triggers a full write stall",
                     new Int64Parameter(&_level0StopTrigger));

  options->addOption(
      "--rocksdb.num-threads-priority-high",
      "number of threads for high priority operations (e.g. flush)",
      new UInt32Parameter(&_numThreadsHigh));

  options->addOption(
      "--rocksdb.num-threads-priority-low",
      "number of threads for low priority operations (e.g. compaction)",
      new UInt32Parameter(&_numThreadsLow));

  options->addOption("--rocksdb.block-cache-size",
                     "size of block cache in bytes", new UInt64Parameter(&_blockCacheSize),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption(
      "--rocksdb.block-cache-shard-bits",
      "number of shard bits to use for block cache (use -1 for default value)",
      new Int64Parameter(&_blockCacheShardBits));

  options->addOption("--rocksdb.enforce-block-cache-size-limit",
                     "if true, strictly enforces the block cache size limit",
                     new BooleanParameter(&_enforceBlockCacheSizeLimit));

  options->addOption(
      "--rocksdb.cache-index-and-filter-blocks",
      "if turned on, the RocksDB block cache quota will also include RocksDB memtable sizes",
      new BooleanParameter(&_cacheIndexAndFilterBlocks),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
  .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.cache-index-and-filter-blocks-with-high-priority",
      "if true and `--rocksdb.cache-index-and-filter-blocks` is also true, cache index and filter blocks with high priority, "
      "making index and filter blocks be less likely to be evicted than data blocks",
      new BooleanParameter(&_cacheIndexAndFilterBlocksWithHighPriority),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
  .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.pin-l0-filter-and-index-blocks-in-cache",
      "if true and `--rocksdb.cache-index-and-filter-blocks` is also true, "
      "filter and index blocks are pinned and only evicted from cache when the table reader is freed",
      new BooleanParameter(&_pinl0FilterAndIndexBlocksInCache),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
  .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.pin-top-level-index-and-filter",
      "If true and `--rocksdb.cache-index-and-filter-blocks` is also true, "
      "the top-level index of partitioned filter and index blocks are pinned and only evicted from cache when the table reader is freed",
      new BooleanParameter(&_pinTopLevelIndexAndFilter),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
  .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.table-block-size",
      "approximate size (in bytes) of user data packed per block",
      new UInt64Parameter(&_tableBlockSize));

  options->addOption("--rocksdb.recycle-log-file-num",
                     "if true, keep a pool of log files around for recycling",
                     new BooleanParameter(&_recycleLogFileNum),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "if non-zero, we perform bigger reads when doing compaction. If you're "
      "running RocksDB on spinning disks, you should set this to at least 2MB. "
      "that way RocksDB's compaction is doing sequential instead of random "
      "reads.",
      new UInt64Parameter(&_compactionReadaheadSize));

  options->addOption("--rocksdb.use-file-logging",
                     "use a file-base logger for RocksDB's own logs",
                     new BooleanParameter(&_useFileLogging),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options->addOption("--rocksdb.wal-recovery-skip-corrupted",
                     "skip corrupted records in WAL recovery",
                     new BooleanParameter(&_skipCorrupted),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));

  options
      ->addOption("--rocksdb.limit-open-files-at-startup",
                  "limit the amount of .sst files RocksDB will inspect at "
                  "startup, in order to startup reduce IO",
                  new BooleanParameter(&_limitOpenFilesAtStartup),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30405);

  options
      ->addOption("--rocksdb.allow-fallocate",
                  "if true, allow RocksDB to use fallocate calls. if false, "
                  "fallocate calls are bypassed",
                  new BooleanParameter(&_allowFAllocate),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30405);
  options
      ->addOption("--rocksdb.exclusive-writes",
                  "if true, writes are exclusive. This allows the RocksDB engine to mimic "
                  "the collection locking behavior of the now-removed MMFiles storage engine, "
                  "but will inhibit concurrent write operations",
                  new BooleanParameter(&_exclusiveWrites),
                  arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
      .setIntroducedIn(30504).setDeprecatedIn(30800);

  //////////////////////////////////////////////////////////////////////////////
  /// add column family-specific options now
  //////////////////////////////////////////////////////////////////////////////
  std::initializer_list<RocksDBColumnFamilyManager::Family> families = {
      RocksDBColumnFamilyManager::Family::Definitions,
      RocksDBColumnFamilyManager::Family::Documents,
      RocksDBColumnFamilyManager::Family::PrimaryIndex,
      RocksDBColumnFamilyManager::Family::EdgeIndex,
      RocksDBColumnFamilyManager::Family::VPackIndex,
      RocksDBColumnFamilyManager::Family::GeoIndex,
      RocksDBColumnFamilyManager::Family::FulltextIndex};

  auto addMaxWriteBufferNumberCf = [this, &options](RocksDBColumnFamilyManager::Family family) {
    std::string name =
        RocksDBColumnFamilyManager::name(family, RocksDBColumnFamilyManager::NameMode::External);
    std::size_t index =
        static_cast<std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(family);
    options->addOption("--rocksdb.max-write-buffer-number-" + name,
                       "if non-zero, overrides the value of "
                       "--rocksdb.max-write-buffer-number for the " +
                           name + " column family",
                       new UInt64Parameter(&_maxWriteBufferNumberCf[index]),
                       arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                       .setIntroducedIn(30800);
  };
  for (auto family : families) {
    addMaxWriteBufferNumberCf(family);
  }
}

void RocksDBOptionFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_targetFileSizeMultiplier < 1) {
    LOG_TOPIC("664be", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.target-file-size-multiplier'";
  }
  if (_writeBufferSize > 0 && _writeBufferSize < 1024 * 1024) {
    LOG_TOPIC("4ce44", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.write-buffer-size'";
    FATAL_ERROR_EXIT();
  }
  if (_totalWriteBufferSize > 0 && _totalWriteBufferSize < 64 * 1024 * 1024) {
    LOG_TOPIC("4ab88", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.total-write-buffer-size'";
    FATAL_ERROR_EXIT();
  }
  if (_maxBytesForLevelMultiplier <= 0.0) {
    LOG_TOPIC("f6f8a", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-bytes-for-level-multiplier'";
    FATAL_ERROR_EXIT();
  }
  if (_numLevels < 1 || _numLevels > 20) {
    LOG_TOPIC("70938", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-levels'";
    FATAL_ERROR_EXIT();
  }

  if (_maxBackgroundJobs != -1 && (_maxBackgroundJobs < 1 || _maxBackgroundJobs > 128)) {
    LOG_TOPIC("cfc5a", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-background-jobs'";
    FATAL_ERROR_EXIT();
  }

  if (_numThreadsHigh > 64) {
    LOG_TOPIC("d3105", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-high'";
    FATAL_ERROR_EXIT();
  }
  if (_numThreadsLow > 256) {
    LOG_TOPIC("30c65", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-low'";
    FATAL_ERROR_EXIT();
  }
  if (_maxSubcompactions > _numThreadsLow) {
    _maxSubcompactions = _numThreadsLow;
  }
  if (_blockCacheShardBits >= 20 || _blockCacheShardBits < -1) {
    // -1 is RocksDB default value, but anything less is invalid
    LOG_TOPIC("327a3", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.block-cache-shard-bits'";
    FATAL_ERROR_EXIT();
  }

  _minWriteBufferNumberToMergeTouched = options->processingResult().touched(
      "--rocksdb.min-write-buffer-number-to-merge");
 
  // limit memory usage of agent instances, if not otherwise configured
  if (server().hasFeature<AgencyFeature>()) {
    AgencyFeature& feature = server().getFeature<AgencyFeature>();
    if (feature.activated()) {
      // if we are an agency instance...
      if (!options->processingResult().touched("--rocksdb.block-cache-size")) {
        // restrict block cache size to 1 GB if not set explicitly
        _blockCacheSize = std::min<uint64_t>(_blockCacheSize, uint64_t(1) << 30);
      }
      if (!options->processingResult().touched("--rocksdb.total-write-buffer-size")) {
        // restrict total write buffer size to 512 MB if not set explicitly
        _totalWriteBufferSize = std::min<uint64_t>(_totalWriteBufferSize, uint64_t(512) << 20);
      }
    }
  }
}

void RocksDBOptionFeature::start() {
  uint32_t max = _maxBackgroundJobs / 2;
  uint32_t clamped = std::max(std::min(static_cast<uint32_t>(NumberOfCores::getValue()), max), 1U);
  // lets test this out
  if (_numThreadsHigh == 0) {
    _numThreadsHigh = clamped;
  }
  if (_numThreadsLow == 0) {
    _numThreadsLow = clamped;
  }

  LOG_TOPIC("f66e4", TRACE, Logger::ROCKSDB)
      << "using RocksDB options:"
      << " wal_dir: '" << _walDirectory << "'"
      << ", write_buffer_size: " << _writeBufferSize
      << ", total_write_buffer_size: " << _totalWriteBufferSize
      << ", max_write_buffer_number: " << _maxWriteBufferNumber
      << ", max_write_buffer_size_to_maintain: " << _maxWriteBufferSizeToMaintain
      << ", max_total_wal_size: " << _maxTotalWalSize
      << ", delayed_write_rate: " << _delayedWriteRate
      << ", min_write_buffer_number_to_merge: " << _minWriteBufferNumberToMerge
      << ", num_levels: " << _numLevels << ", num_uncompressed_levels: " << _numUncompressedLevels
      << ", max_bytes_for_level_base: " << _maxBytesForLevelBase
      << ", max_bytes_for_level_multiplier: " << _maxBytesForLevelMultiplier
      << ", max_background_jobs: " << _maxBackgroundJobs
      << ", max_sub_compactions: " << _maxSubcompactions
      << ", target_file_size_base: " << _targetFileSizeBase 
      << ", target_file_size_multiplier: " << _targetFileSizeMultiplier
      << ", num_threads_high: " << _numThreadsHigh
      << ", num_threads_low: " << _numThreadsLow << ", block_cache_size: " << _blockCacheSize
      << ", block_cache_shard_bits: " << _blockCacheShardBits
      << ", block_cache_strict_capacity_limit: " << std::boolalpha << _enforceBlockCacheSizeLimit
      << ", cache_index_and_filter_blocks: " << std::boolalpha << _cacheIndexAndFilterBlocks
      << ", cache_index_and_filter_blocks_with_high_priority: " << std::boolalpha << _cacheIndexAndFilterBlocksWithHighPriority
      << ", pin_l0_filter_and_index_blocks_in_cache: " << std::boolalpha << _pinl0FilterAndIndexBlocksInCache
      << ", pin_top_level_index_and_filter: " << std::boolalpha << _pinTopLevelIndexAndFilter
      << ", table_block_size: " << _tableBlockSize
      << ", recycle_log_file_num: " << std::boolalpha << _recycleLogFileNum
      << ", compaction_read_ahead_size: " << _compactionReadaheadSize
      << ", level0_compaction_trigger: " << _level0CompactionTrigger
      << ", level0_slowdown_trigger: " << _level0SlowdownTrigger
      << ", enable_pipelined_write: " << _enablePipelinedWrite
      << ", optimize_filters_for_hits: " << std::boolalpha << _optimizeFiltersForHits
      << ", use_direct_reads: " << std::boolalpha << _useDirectReads
      << ", use_direct_io_for_flush_and_compaction: " << std::boolalpha
      << _useDirectIoForFlushAndCompaction << ", use_fsync: " << std::boolalpha
      << _useFSync << ", allow_fallocate: " << std::boolalpha << _allowFAllocate
      << ", max_open_files limit: " << std::boolalpha << _limitOpenFilesAtStartup
      << ", dynamic_level_bytes: " << std::boolalpha << _dynamicLevelBytes;
}

rocksdb::ColumnFamilyOptions RocksDBOptionFeature::columnFamilyOptions(
    RocksDBColumnFamilyManager::Family family, rocksdb::Options const& base,
    rocksdb::BlockBasedTableOptions const& tableBase) const {
  rocksdb::ColumnFamilyOptions options(base);

  switch (family) {
    case RocksDBColumnFamilyManager::Family::Definitions:
    case RocksDBColumnFamilyManager::Family::Invalid:
      break;

    case RocksDBColumnFamilyManager::Family::Documents:
    case RocksDBColumnFamilyManager::Family::PrimaryIndex:
    case RocksDBColumnFamilyManager::Family::GeoIndex:
    case RocksDBColumnFamilyManager::Family::FulltextIndex: {
      // fixed 8 byte object id prefix
      options.prefix_extractor = std::shared_ptr<rocksdb::SliceTransform const>(
          rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));
      break;
    }

    case RocksDBColumnFamilyManager::Family::EdgeIndex: {
      options.prefix_extractor = std::make_shared<RocksDBPrefixExtractor>();
      // also use hash-search based SST file format
      rocksdb::BlockBasedTableOptions tableOptions(tableBase);
      tableOptions.index_type = rocksdb::BlockBasedTableOptions::IndexType::kHashSearch;
      options.table_factory = std::shared_ptr<rocksdb::TableFactory>(
          rocksdb::NewBlockBasedTableFactory(tableOptions));
      break;
    }
    case RocksDBColumnFamilyManager::Family::VPackIndex: {
      // velocypack based index variants with custom comparator
      rocksdb::BlockBasedTableOptions tableOptions(tableBase);
      tableOptions.filter_policy.reset();  // intentionally no bloom filter here
      options.table_factory = std::shared_ptr<rocksdb::TableFactory>(
          rocksdb::NewBlockBasedTableFactory(tableOptions));
      options.comparator = _vpackCmp.get();
      break;
    }
  }

  // override
  std::size_t index =
      static_cast<std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(family);
  TRI_ASSERT(index < _maxWriteBufferNumberCf.size());
  if (_maxWriteBufferNumberCf[index] > 0) {
    options.max_write_buffer_number = static_cast<int>(_maxWriteBufferNumberCf[index]);
  }
  if (!_minWriteBufferNumberToMergeTouched) {
    options.min_write_buffer_number_to_merge = static_cast<int>(
        defaultMinWriteBufferNumberToMerge(_totalWriteBufferSize, _writeBufferSize,
                                           options.max_write_buffer_number));
  }

  return options;
}
