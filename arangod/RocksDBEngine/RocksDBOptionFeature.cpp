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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cstddef>
#include <ios>
#include <limits>
#include <memory>

#include "RocksDBOptionFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Agency/AgencyFeature.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Basics/process-utils.h"
#include "Basics/system-functions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"

#include <rocksdb/advanced_options.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/options.h>
#include <rocksdb/slice_transform.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
std::unordered_set<std::string> const compressionTypes = {
    {"snappy"}, {"lz4"}, {"lz4hc"}, {"none"}};

rocksdb::TransactionDBOptions rocksDBTrxDefaults;
rocksdb::Options rocksDBDefaults;
rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;

// minimum size of a block cache shard. we want to at least store
// that much data in each shard (rationale: a data block read from
// disk must fit into the block cache if the block cache's strict
// capacity limit is set. otherwise the block cache will fail reads
// with Status::Incomplete() or Status::MemoryLimit()).
constexpr uint64_t minShardSize = 128 * 1024 * 1024;

uint64_t defaultBlockCacheSize() {
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(4) << 30)) {
    // if we have at least 4GB of RAM, the default size is (RAM - 2GB) * 0.3
    return static_cast<uint64_t>(
        (PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.3);
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
    return static_cast<uint64_t>(
        (PhysicalMemory::getValue() - (static_cast<uint64_t>(2) << 30)) * 0.4);
  }
  if (PhysicalMemory::getValue() >= (static_cast<uint64_t>(1) << 30)) {
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
            RocksDBColumnFamilyManager::numberOfColumnFamilies >
        totalSize) {
      break;
    }

    safe = test;
  }

  return safe;
}

}  // namespace

RocksDBOptionFeature::RocksDBOptionFeature(Server& server)
    : ArangodFeature{server, *this},
      // number of lock stripes for the transaction lock manager. we bump this
      // to at least 16 to reduce contention for small scale systems.
      _transactionLockStripes(
          std::max(NumberOfCores::getValue(), std::size_t(16))),
      _transactionLockTimeout(rocksDBTrxDefaults.transaction_lock_timeout),
      _compressionType("lz4"),
      _totalWriteBufferSize(rocksDBDefaults.db_write_buffer_size),
      _writeBufferSize(rocksDBDefaults.write_buffer_size),
      _maxWriteBufferNumber(8 + 2),  // number of column families plus 2
      _maxWriteBufferSizeToMaintain(0),
      _maxTotalWalSize(80 << 20),
      _delayedWriteRate(rocksDBDefaults.delayed_write_rate),
      _minWriteBufferNumberToMerge(defaultMinWriteBufferNumberToMerge(
          _totalWriteBufferSize, _writeBufferSize, _maxWriteBufferNumber)),
      _numLevels(rocksDBDefaults.num_levels),
      _numUncompressedLevels(2),
      _maxBytesForLevelBase(rocksDBDefaults.max_bytes_for_level_base),
      _maxBytesForLevelMultiplier(
          rocksDBDefaults.max_bytes_for_level_multiplier),
      _maxBackgroundJobs(rocksDBDefaults.max_background_jobs),
      _maxSubcompactions(2),
      _numThreadsHigh(0),
      _numThreadsLow(0),
      _targetFileSizeBase(rocksDBDefaults.target_file_size_base),
      _targetFileSizeMultiplier(rocksDBDefaults.target_file_size_multiplier),
      _blockCacheSize(::defaultBlockCacheSize()),
      _blockCacheShardBits(-1),
      _tableBlockSize(std::max(
          rocksDBTableOptionsDefaults.block_size,
          static_cast<decltype(rocksDBTableOptionsDefaults.block_size)>(16 *
                                                                        1024))),
      _compactionReadaheadSize(
          2 * 1024 * 1024),  // rocksDBDefaults.compaction_readahead_size
      _level0CompactionTrigger(2),
      _level0SlowdownTrigger(16),
      _level0StopTrigger(256),
      _pendingCompactionBytesSlowdownTrigger(128 * 1024ull),
      _pendingCompactionBytesStopTrigger(16 * 1073741824ull),
      // note: this is a default value from RocksDB (db/column_family.cc,
      // kAdjustedTtl):
      _periodicCompactionTtl(30 * 24 * 60 * 60),
      _checksumType("xxHash64"),
      _compactionStyle("level"),
      _formatVersion(5),
      _enableIndexCompression(
          rocksDBTableOptionsDefaults.enable_index_compression),
      _prepopulateBlockCache(false),
      _reserveTableBuilderMemory(
          rocksDBTableOptionsDefaults.reserve_table_builder_memory),
      _reserveTableReaderMemory(
          rocksDBTableOptionsDefaults.reserve_table_reader_memory),
      _recycleLogFileNum(rocksDBDefaults.recycle_log_file_num),
      _enforceBlockCacheSizeLimit(true),
      _cacheIndexAndFilterBlocks(true),
      _cacheIndexAndFilterBlocksWithHighPriority(
          rocksDBTableOptionsDefaults
              .cache_index_and_filter_blocks_with_high_priority),
      _pinl0FilterAndIndexBlocksInCache(
          rocksDBTableOptionsDefaults.pin_l0_filter_and_index_blocks_in_cache),
      _pinTopLevelIndexAndFilter(
          rocksDBTableOptionsDefaults.pin_top_level_index_and_filter),
      _blockAlignDataBlocks(rocksDBTableOptionsDefaults.block_align),
      _enablePipelinedWrite(true),
      _optimizeFiltersForHits(rocksDBDefaults.optimize_filters_for_hits),
      _useDirectReads(rocksDBDefaults.use_direct_reads),
      _useDirectIoForFlushAndCompaction(
          rocksDBDefaults.use_direct_io_for_flush_and_compaction),
      _useFSync(rocksDBDefaults.use_fsync),
      _skipCorrupted(false),
      _dynamicLevelBytes(true),
      _enableStatistics(false),
      _useFileLogging(false),
      _limitOpenFilesAtStartup(false),
      _allowFAllocate(true),
      _exclusiveWrites(false),
      _minWriteBufferNumberToMergeTouched(false),
      _maxWriteBufferNumberCf{0, 0, 0, 0, 0, 0, 0} {
  // setting the number of background jobs to
  _maxBackgroundJobs = static_cast<int32_t>(
      std::max(static_cast<size_t>(2), NumberOfCores::getValue()));

  if (_totalWriteBufferSize == 0) {
    // unlimited write buffer size... now set to some fraction of physical RAM
    _totalWriteBufferSize = ::defaultTotalWriteBufferSize();
  }

  setOptional(true);
  startsAfter<BasicFeaturePhaseServer>();
}

void RocksDBOptionFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "RocksDB engine");

  options->addObsoleteOption("--rocksdb.enabled",
                             "obsolete always active - Whether or not the "
                             "RocksDB engine is enabled for the persistent "
                             "index",
                             true);

  options->addOption("--rocksdb.wal-directory",
                     "optional path to the RocksDB WAL directory. "
                     "If not set, the WAL directory will be located inside the "
                     "regular data directory",
                     new StringParameter(&_walDirectory),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options
      ->addOption("--rocksdb.target-file-size-base",
                  "Per-file target file size for compaction (in bytes). The "
                  "actual target file size for each level is "
                  "`--rocksdb.target-file-size-base` multiplied by "
                  "`--rocksdb.target-file-size-multiplier` ^ (level - 1)",
                  new UInt64Parameter(&_targetFileSizeBase),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options
      ->addOption(
          "--rocksdb.target-file-size-multiplier",
          "multiplier for `--rocksdb.target-file-size`, a value of 1 means "
          "that files in different levels will have the same size",
          new UInt64Parameter(&_targetFileSizeMultiplier, /*base*/ 1,
                              /*minValue*/ 1),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options
      ->addOption("--rocksdb.compression-type",
                  "compression algorithm to use within RocksDB",
                  new DiscreteValuesParameter<StringParameter>(
                      &_compressionType, ::compressionTypes))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.transaction-lock-stripes",
                  "Number of lock stripes to use for transaction locks",
                  new UInt64Parameter(&_transactionLockStripes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30902);

  options->addOption(
      "--rocksdb.transaction-lock-timeout",
      "If positive, specifies the wait timeout in milliseconds when "
      " a transaction attempts to lock a document. A negative value "
      "is not recommended as it can lead to deadlocks (0 = no waiting, < 0 no "
      "timeout)",
      new Int64Parameter(&_transactionLockTimeout),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.total-write-buffer-size",
      "maximum total size of in-memory write buffers (0 = unbounded)",
      new UInt64Parameter(&_totalWriteBufferSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Dynamic,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.write-buffer-size",
                     "amount of data to build up in memory before converting "
                     "to a sorted on-disk file (0 = disabled)",
                     new UInt64Parameter(&_writeBufferSize),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.max-write-buffer-number",
      "maximum number of write buffers that build up in memory "
      "(default: number of column families + 2 = 9 write buffers). "
      "You can increase the amount only",
      new UInt64Parameter(&_maxWriteBufferNumber),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options
      ->addOption(
          "--rocksdb.max-write-buffer-size-to-maintain",
          "maximum size of immutable write buffers that build up in memory "
          "per column family (larger values mean that more in-memory data "
          "can be used for transaction conflict checking (-1 = use automatic "
          "default value, "
          "0 = do not keep immutable flushed write buffers, which is the "
          "default and usually "
          "correct))",
          new Int64Parameter(&_maxWriteBufferSizeToMaintain),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30703);

  options->addOption("--rocksdb.max-total-wal-size",
                     "maximum total size of WAL files that will force flush "
                     "stale column families",
                     new UInt64Parameter(&_maxTotalWalSize),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.delayed-write-rate",
      "limited write rate to DB (in bytes per second) if we are writing to the "
      "last mem-table allowed and we allow more than 3 mem-tables, or if we "
      "have surpassed a certain number of level-0 files and need to slowdown "
      "writes",
      new UInt64Parameter(&_delayedWriteRate),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOldOption("rocksdb.delayed_write_rate",
                        "rocksdb.delayed-write-rate");

  options->addOption("--rocksdb.min-write-buffer-number-to-merge",
                     "minimum number of write buffers that will be merged "
                     "together before writing to storage",
                     new UInt64Parameter(&_minWriteBufferNumberToMerge),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Dynamic,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.num-levels",
                     "number of levels for the database",
                     new UInt64Parameter(&_numLevels, /*base*/ 1,
                                         /*minValue*/ 1, /*maxValue*/ 20),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.num-uncompressed-levels",
                     "number of uncompressed levels for the database",
                     new UInt64Parameter(&_numUncompressedLevels),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.dynamic-level-bytes",
                     "if true, determine the number of bytes for each level "
                     "dynamically to minimize space amplification",
                     new BooleanParameter(&_dynamicLevelBytes),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.max-bytes-for-level-base",
                     "if not using dynamic level sizes, this controls the "
                     "maximum total data size for level-1",
                     new UInt64Parameter(&_maxBytesForLevelBase),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.max-bytes-for-level-multiplier",
      "if not using dynamic level sizes, the maximum number of "
      "bytes for level L can be calculated as "
      " max-bytes-for-level-base * "
      "(max-bytes-for-level-multiplier ^ (L-1))",
      new DoubleParameter(&_maxBytesForLevelMultiplier, /*base*/ 1.0,
                          /*minValue*/ 0.0,
                          /*maxValue*/ std::numeric_limits<double>::max(),
                          /*minInclusive*/ false),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.block-align-data-blocks",
      "if true, aligns data blocks on lesser of page size and block size",
      new BooleanParameter(&_blockAlignDataBlocks),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.enable-pipelined-write",
      "if true, use a two stage write queue for WAL writes and memtable writes",
      new BooleanParameter(&_enablePipelinedWrite),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.enable-statistics",
                     "whether or not RocksDB statistics should be turned on",
                     new BooleanParameter(&_enableStatistics),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for "
      "keys missed. This would be used in cases where the application knows "
      "that there are very few misses or the performance in the case of "
      "misses is not important",
      new BooleanParameter(&_optimizeFiltersForHits),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

#ifdef __linux__
  options->addOption(
      "--rocksdb.use-direct-reads", "use O_DIRECT for reading files",
      new BooleanParameter(&_useDirectReads),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs,
                                   arangodb::options::Flags::OsLinux,
                                   arangodb::options::Flags::Uncommon));

  options->addOption(
      "--rocksdb.use-direct-io-for-flush-and-compaction",
      "use O_DIRECT for flush and compaction",
      new BooleanParameter(&_useDirectIoForFlushAndCompaction),
      arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs,
                                   arangodb::options::Flags::OsLinux,
                                   arangodb::options::Flags::Uncommon));
#endif

  options->addOption("--rocksdb.use-fsync",
                     "issue an fsync when writing to disk (set to true "
                     "for issuing fdatasync only)",
                     new BooleanParameter(&_useFSync),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.max-background-jobs",
      "Maximum number of concurrent background jobs (compactions and flushes)",
      new Int32Parameter(&_maxBackgroundJobs),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon, arangodb::options::Flags::Dynamic,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.max-subcompactions",
                     "maximum number of concurrent subjobs for a background "
                     "compaction",
                     new UInt32Parameter(&_maxSubcompactions),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.level0-compaction-trigger",
                     "number of level-0 files that triggers a compaction",
                     new Int64Parameter(&_level0CompactionTrigger),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.level0-slowdown-trigger",
                     "number of level-0 files that triggers a write slowdown",
                     new Int64Parameter(&_level0SlowdownTrigger),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.level0-stop-trigger",
                     "number of level-0 files that triggers a full write stop",
                     new Int64Parameter(&_level0StopTrigger),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options
      ->addOption(
          "--rocksdb.pending-compactions-slowdown-trigger",
          "number of pending compaction bytes that triggers a write slowdown",
          new UInt64Parameter(&_pendingCompactionBytesSlowdownTrigger),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805);

  options
      ->addOption(
          "--rocksdb.pending-compactions-stop-trigger",
          "number of pending compaction bytes that triggers a full write stop",
          new UInt64Parameter(&_pendingCompactionBytesStopTrigger),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805);

  options->addOption(
      "--rocksdb.num-threads-priority-high",
      "number of threads for high priority operations (e.g. flush)",
      new UInt32Parameter(&_numThreadsHigh, /*base*/ 1, /*minValue*/ 0,
                          /*maxValue*/ 64),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.num-threads-priority-low",
      "number of threads for low priority operations (e.g. compaction)",
      new UInt32Parameter(&_numThreadsLow, /*base*/ 1, /*minValue*/ 0,
                          /*maxValue*/ 256),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.block-cache-size",
                     "size of block cache in bytes",
                     new UInt64Parameter(&_blockCacheSize),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Dynamic,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.block-cache-shard-bits",
      "number of shard bits to use for block cache (use -1 for default value)",
      new Int64Parameter(&_blockCacheShardBits, /*base*/ 1, /*minValue*/ -1,
                         /*maxValue*/ 20, /*minInclusive*/ true,
                         /*maxInclusive*/ false),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.enforce-block-cache-size-limit",
                     "if true, strictly enforces the block cache size limit",
                     new BooleanParameter(&_enforceBlockCacheSizeLimit),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options
      ->addOption(
          "--rocksdb.cache-index-and-filter-blocks",
          "if turned on, the RocksDB block cache quota will also include "
          "RocksDB memtable sizes",
          new BooleanParameter(&_cacheIndexAndFilterBlocks),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options
      ->addOption(
          "--rocksdb.cache-index-and-filter-blocks-with-high-priority",
          "if true and `--rocksdb.cache-index-and-filter-blocks` is also true, "
          "cache index and filter blocks with high priority, "
          "making index and filter blocks be less likely to be evicted than "
          "data blocks",
          new BooleanParameter(&_cacheIndexAndFilterBlocksWithHighPriority),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options
      ->addOption(
          "--rocksdb.pin-l0-filter-and-index-blocks-in-cache",
          "if true and `--rocksdb.cache-index-and-filter-blocks` is also true, "
          "filter and index blocks are pinned and only evicted from cache when "
          "the table reader is freed",
          new BooleanParameter(&_pinl0FilterAndIndexBlocksInCache),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options
      ->addOption(
          "--rocksdb.pin-top-level-index-and-filter",
          "If true and `--rocksdb.cache-index-and-filter-blocks` is also true, "
          "the top-level index of partitioned filter and index blocks are "
          "pinned and only evicted from cache when the table reader is freed",
          new BooleanParameter(&_pinTopLevelIndexAndFilter),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30701);

  options->addOption(
      "--rocksdb.table-block-size",
      "approximate size (in bytes) of user data packed per block",
      new UInt64Parameter(&_tableBlockSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.recycle-log-file-num",
                     "if true, keep a pool of log files around for recycling",
                     new BooleanParameter(&_recycleLogFileNum),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "if non-zero, we perform bigger reads when doing compaction. If you're "
      "running RocksDB on spinning disks, you should set this to at least 2MB. "
      "that way RocksDB's compaction is doing sequential instead of random "
      "reads.",
      new UInt64Parameter(&_compactionReadaheadSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.use-file-logging",
                     "use a file-base logger for RocksDB's own logs",
                     new BooleanParameter(&_useFileLogging),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options->addOption("--rocksdb.wal-recovery-skip-corrupted",
                     "skip corrupted records in WAL recovery",
                     new BooleanParameter(&_skipCorrupted),
                     arangodb::options::makeFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::DefaultNoComponents,
                         arangodb::options::Flags::OnAgent,
                         arangodb::options::Flags::OnDBServer,
                         arangodb::options::Flags::OnSingle));

  options
      ->addOption("--rocksdb.limit-open-files-at-startup",
                  "limit the amount of .sst files RocksDB will inspect at "
                  "startup, in order to reduce startup IO",
                  new BooleanParameter(&_limitOpenFilesAtStartup),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30405);

  options
      ->addOption("--rocksdb.allow-fallocate",
                  "if true, allow RocksDB to use fallocate calls. if false, "
                  "fallocate calls are bypassed",
                  new BooleanParameter(&_allowFAllocate),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30405);

  options
      ->addOption(
          "--rocksdb.exclusive-writes",
          "if true, writes are exclusive. This allows the RocksDB engine to "
          "mimic "
          "the collection locking behavior of the now-removed MMFiles storage "
          "engine, "
          "but will inhibit concurrent write operations",
          new BooleanParameter(&_exclusiveWrites),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30504)
      .setDeprecatedIn(30800);

  std::unordered_set<std::string> checksumTypes = {"crc32c", "xxHash",
                                                   "xxHash64", "XXH3"};
  TRI_ASSERT(checksumTypes.contains(_checksumType));
  options
      ->addOption("--rocksdb.checksum-type",
                  "checksum type to use for table files",
                  new DiscreteValuesParameter<StringParameter>(&_checksumType,
                                                               checksumTypes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  std::unordered_set<std::string> compactionStyles = {"level", "universal",
                                                      "fifo", "none"};
  TRI_ASSERT(compactionStyles.contains(_compactionStyle));
  options
      ->addOption("--rocksdb.compaction-style",
                  "compaction style which is used to pick the next file(s) to "
                  "be compacted",
                  new DiscreteValuesParameter<StringParameter>(
                      &_compactionStyle, compactionStyles),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  std::unordered_set<uint32_t> formatVersions = {3, 4, 5};
  options
      ->addOption("--rocksdb.format-version",
                  "table format version to use inside RocksDB",
                  new DiscreteValuesParameter<UInt32Parameter>(&_formatVersion,
                                                               formatVersions),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.enable-index-compression",
                  "enable index compression",
                  new BooleanParameter(&_enableIndexCompression),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.prepopulate-block-cache",
                  "prepopulate block cache on flushes",
                  new BooleanParameter(&_prepopulateBlockCache),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.reserve-table-builder-memory",
                  "account for table building memory in block cache",
                  new BooleanParameter(&_reserveTableBuilderMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.reserve-table-reader-memory",
                  "account for table reader memory in block cache",
                  new BooleanParameter(&_reserveTableReaderMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  options
      ->addOption("--rocksdb.periodic-compaction-ttl",
                  "TTL (in seconds) for periodic compaction of .sst files, "
                  "based on file age (0 = no periodic compaction)",
                  new UInt64Parameter(&_periodicCompactionTtl))
      .setIntroducedIn(30903);

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
      RocksDBColumnFamilyManager::Family::FulltextIndex,
      RocksDBColumnFamilyManager::Family::ReplicatedLogs};

  auto addMaxWriteBufferNumberCf =
      [this, &options](RocksDBColumnFamilyManager::Family family) {
        std::string name = RocksDBColumnFamilyManager::name(
            family, RocksDBColumnFamilyManager::NameMode::External);
        std::size_t index = static_cast<
            std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(
            family);
        options
            ->addOption("--rocksdb.max-write-buffer-number-" + name,
                        "If non-zero, overrides the value of "
                        "`--rocksdb.max-write-buffer-number` for the " +
                            name + " column family",
                        new UInt64Parameter(&_maxWriteBufferNumberCf[index]),
                        arangodb::options::makeDefaultFlags(
                            arangodb::options::Flags::Uncommon))
            .setIntroducedIn(30800);
      };
  for (auto family : families) {
    addMaxWriteBufferNumberCf(family);
  }
}

void RocksDBOptionFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
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
  if (_maxBackgroundJobs != -1 &&
      (_maxBackgroundJobs < 1 || _maxBackgroundJobs > 128)) {
    LOG_TOPIC("cfc5a", FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-background-jobs'";
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
        _blockCacheSize =
            std::min<uint64_t>(_blockCacheSize, uint64_t(1) << 30);
      }
      if (!options->processingResult().touched(
              "--rocksdb.total-write-buffer-size")) {
        // restrict total write buffer size to 512 MB if not set explicitly
        _totalWriteBufferSize =
            std::min<uint64_t>(_totalWriteBufferSize, uint64_t(512) << 20);
      }
    }
  }

  if (_enforceBlockCacheSizeLimit && !options->processingResult().touched(
                                         "--rocksdb.block-cache-shard-bits")) {
    // if block cache size limit is enforced, and the number of shard bits for
    // the block cache hasn't been set, we set it dynamically:
    // we would like that each block cache shard can hold data blocks of
    // at least a common size. Rationale: data blocks can be quite large. if
    // they don't fit into the block cache upon reading, the block cache will
    // return Status::Incomplete() or Status::MemoryLimit() when the block
    // cache's strict capacity limit is set. then we cannot read data anymore.
    // we are limiting the maximum number of shard bits to 10 here, which is
    // 1024 shards. that should be enough shards even for very big caches.
    // note that RocksDB also has an internal upper bound for the number of
    // shards bits, which is 20.
    _blockCacheShardBits = std::clamp(
        int64_t(std::floor(
            std::log2(static_cast<double>(_blockCacheSize) / ::minShardSize))),
        int64_t(1), int64_t(10));
  }
}

void RocksDBOptionFeature::prepare() {
  if (_enforceBlockCacheSizeLimit && _blockCacheSize > 0) {
    uint64_t shardSize =
        _blockCacheSize / (uint64_t(1) << _blockCacheShardBits);
    // if we can't store a data block of the mininmum size in the block cache,
    // we may run into problems when trying to put a large data block into the
    // cache. in this case the block cache may return a Status::Incomplete()
    // or Status::MemoryLimit() error and fail the entire read.
    // warn the user about it!
    if (shardSize < ::minShardSize) {
      LOG_TOPIC("31d7c", WARN, Logger::ROCKSDB)
          << "size of RocksDB block cache shards seems to be too low. "
          << "block cache size: " << _blockCacheSize
          << ", shard bits: " << _blockCacheShardBits
          << ", shard size: " << shardSize
          << ". it is probably useful to set "
             "`--rocksdb.enforce-block-cache-size-limit` to false "
          << "to avoid incomplete cache reads.";
    }
  }
}

void RocksDBOptionFeature::start() {
  uint32_t max = _maxBackgroundJobs / 2;
  uint32_t clamped = std::max(
      std::min(static_cast<uint32_t>(NumberOfCores::getValue()), max), 1U);
  // lets test this out
  if (_numThreadsHigh == 0) {
    _numThreadsHigh = clamped;
  }
  if (_numThreadsLow == 0) {
    _numThreadsLow = clamped;
  }

  if (_maxSubcompactions > _numThreadsLow) {
    if (server().options()->processingResult().touched(
            "--rocksdb.max-subcompactions")) {
      LOG_TOPIC("e7c85", WARN, Logger::ENGINES)
          << "overriding value for option `--rocksdb.max-subcompactions` to "
          << _numThreadsLow
          << " because the specified value is greater than the number of "
             "threads for low priority operations";
    }
    _maxSubcompactions = _numThreadsLow;
  }

  LOG_TOPIC("f66e4", TRACE, Logger::ROCKSDB)
      << "using RocksDB options:"
      << " wal_dir: '" << _walDirectory << "'"
      << ", compression type: " << _compressionType
      << ", write_buffer_size: " << _writeBufferSize
      << ", total_write_buffer_size: " << _totalWriteBufferSize
      << ", max_write_buffer_number: " << _maxWriteBufferNumber
      << ", max_write_buffer_size_to_maintain: "
      << _maxWriteBufferSizeToMaintain
      << ", max_total_wal_size: " << _maxTotalWalSize
      << ", delayed_write_rate: " << _delayedWriteRate
      << ", min_write_buffer_number_to_merge: " << _minWriteBufferNumberToMerge
      << ", num_levels: " << _numLevels
      << ", num_uncompressed_levels: " << _numUncompressedLevels
      << ", max_bytes_for_level_base: " << _maxBytesForLevelBase
      << ", max_bytes_for_level_multiplier: " << _maxBytesForLevelMultiplier
      << ", max_background_jobs: " << _maxBackgroundJobs
      << ", max_sub_compactions: " << _maxSubcompactions
      << ", target_file_size_base: " << _targetFileSizeBase
      << ", target_file_size_multiplier: " << _targetFileSizeMultiplier
      << ", num_threads_high: " << _numThreadsHigh
      << ", num_threads_low: " << _numThreadsLow
      << ", block_cache_size: " << _blockCacheSize
      << ", block_cache_shard_bits: " << _blockCacheShardBits
      << ", block_cache_strict_capacity_limit: " << std::boolalpha
      << _enforceBlockCacheSizeLimit
      << ", cache_index_and_filter_blocks: " << std::boolalpha
      << _cacheIndexAndFilterBlocks
      << ", cache_index_and_filter_blocks_with_high_priority: "
      << std::boolalpha << _cacheIndexAndFilterBlocksWithHighPriority
      << ", pin_l0_filter_and_index_blocks_in_cache: " << std::boolalpha
      << _pinl0FilterAndIndexBlocksInCache
      << ", pin_top_level_index_and_filter: " << std::boolalpha
      << _pinTopLevelIndexAndFilter << ", table_block_size: " << _tableBlockSize
      << ", recycle_log_file_num: " << std::boolalpha << _recycleLogFileNum
      << ", compaction_read_ahead_size: " << _compactionReadaheadSize
      << ", level0_compaction_trigger: " << _level0CompactionTrigger
      << ", level0_slowdown_trigger: " << _level0SlowdownTrigger
      << ", periodic_compaction_ttl: " << _periodicCompactionTtl
      << ", checksum: " << _checksumType
      << ", format_version: " << _formatVersion
      << ", enable_index_compression: " << _enableIndexCompression
      << ", prepopulate_block_cache: " << _prepopulateBlockCache
      << ", reserve_table_builder_memory: " << _reserveTableBuilderMemory
      << ", reserve_reader_builder_memory: " << _reserveTableReaderMemory
      << ", enable_pipelined_write: " << _enablePipelinedWrite
      << ", optimize_filters_for_hits: " << _optimizeFiltersForHits
      << ", use_direct_reads: " << _useDirectReads
      << ", use_direct_io_for_flush_and_compaction: "
      << _useDirectIoForFlushAndCompaction << ", use_fsync: " << _useFSync
      << ", allow_fallocate: " << _allowFAllocate
      << ", max_open_files limit: " << _limitOpenFilesAtStartup
      << ", dynamic_level_bytes: " << _dynamicLevelBytes;
}

rocksdb::TransactionDBOptions RocksDBOptionFeature::getTransactionDBOptions()
    const {
  rocksdb::TransactionDBOptions result;
  // number of locks per column_family
  result.num_stripes =
      std::max(size_t(1), static_cast<size_t>(_transactionLockStripes));
  result.transaction_lock_timeout = _transactionLockTimeout;
  return result;
}

rocksdb::Options RocksDBOptionFeature::doGetOptions() const {
  rocksdb::Options result;
  result.allow_fallocate = _allowFAllocate;
  result.enable_pipelined_write = _enablePipelinedWrite;
  result.write_buffer_size = static_cast<size_t>(_writeBufferSize);
  result.max_write_buffer_number = static_cast<int>(_maxWriteBufferNumber);
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
  result.max_write_buffer_size_to_maintain = _maxWriteBufferSizeToMaintain;
  result.delayed_write_rate = _delayedWriteRate;
  result.min_write_buffer_number_to_merge =
      static_cast<int>(_minWriteBufferNumberToMerge);
  result.num_levels = static_cast<int>(_numLevels);
  result.level_compaction_dynamic_level_bytes = _dynamicLevelBytes;
  result.max_bytes_for_level_base = _maxBytesForLevelBase;
  result.max_bytes_for_level_multiplier =
      static_cast<int>(_maxBytesForLevelMultiplier);
  result.optimize_filters_for_hits = _optimizeFiltersForHits;
  result.use_direct_reads = _useDirectReads;
  result.use_direct_io_for_flush_and_compaction =
      _useDirectIoForFlushAndCompaction;

  result.target_file_size_base = _targetFileSizeBase;
  result.target_file_size_multiplier =
      static_cast<int>(_targetFileSizeMultiplier);
  // during startup, limit the total WAL size to a small value so we do not see
  // large WAL files created at startup.
  // Instead, we will start with a small value here and up it later in the
  // startup process
  result.max_total_wal_size = 4 * 1024 * 1024;

  result.wal_dir = _walDirectory;

  if (_skipCorrupted) {
    result.wal_recovery_mode =
        rocksdb::WALRecoveryMode::kSkipAnyCorruptedRecords;
  } else {
    result.wal_recovery_mode = rocksdb::WALRecoveryMode::kPointInTimeRecovery;
  }

  result.max_background_jobs = static_cast<int>(_maxBackgroundJobs);
  result.max_subcompactions = _maxSubcompactions;
  result.use_fsync = _useFSync;

  rocksdb::CompressionType compressionType = rocksdb::kNoCompression;
  if (_compressionType == "none") {
    compressionType = rocksdb::kNoCompression;
  } else if (_compressionType == "snappy") {
    compressionType = rocksdb::kSnappyCompression;
  } else if (_compressionType == "lz4") {
    compressionType = rocksdb::kLZ4Compression;
  } else if (_compressionType == "lz4hc") {
    compressionType = rocksdb::kLZ4HCCompression;
  } else {
    TRI_ASSERT(false);
    LOG_TOPIC("edc91", FATAL, arangodb::Logger::STARTUP)
        << "unexpected compression type '" << _compressionType << "'";
    FATAL_ERROR_EXIT();
  }

  // only compress levels >= 2
  result.compression_per_level.resize(result.num_levels);
  for (int level = 0; level < result.num_levels; ++level) {
    result.compression_per_level[level] =
        (((uint64_t)level >= _numUncompressedLevels) ? compressionType
                                                     : rocksdb::kNoCompression);
  }

  if (_compactionStyle == "level") {
    result.compaction_style = rocksdb::kCompactionStyleLevel;
  } else if (_compactionStyle == "universal") {
    result.compaction_style = rocksdb::kCompactionStyleUniversal;
  } else if (_compactionStyle == "fifo") {
    result.compaction_style = rocksdb::kCompactionStyleFIFO;
  } else if (_compactionStyle == "none") {
    result.compaction_style = rocksdb::kCompactionStyleNone;
  } else {
    TRI_ASSERT(false);
    LOG_TOPIC("edc92", FATAL, arangodb::Logger::STARTUP)
        << "unexpected compaction style '" << _compactionStyle << "'";
    FATAL_ERROR_EXIT();
  }

  // Number of files to trigger level-0 compaction. A value <0 means that
  // level-0 compaction will not be triggered by number of files at all.
  // Default: 4
  result.level0_file_num_compaction_trigger =
      static_cast<int>(_level0CompactionTrigger);

  // Soft limit on number of level-0 files. We start slowing down writes at this
  // point. A value <0 means that no writing slow down will be triggered by
  // number of files in level-0.
  result.level0_slowdown_writes_trigger =
      static_cast<int>(_level0SlowdownTrigger);

  // Maximum number of level-0 files.  We stop writes at this point.
  result.level0_stop_writes_trigger = static_cast<int>(_level0StopTrigger);

  // Soft limit on pending compaction bytes. We start slowing down writes
  // at this point.
  result.soft_pending_compaction_bytes_limit =
      _pendingCompactionBytesSlowdownTrigger;

  // Maximum number of pending compaction bytes. We stop writes at this point.
  result.hard_pending_compaction_bytes_limit =
      _pendingCompactionBytesStopTrigger;

  result.recycle_log_file_num = _recycleLogFileNum;
  result.compaction_readahead_size =
      static_cast<size_t>(_compactionReadaheadSize);

  // intentionally set the RocksDB logger to ERROR because it will
  // log lots of things otherwise
  if (!_useFileLogging) {
    // if we don't use file logging but log into ArangoDB's logfile,
    // we only want real errors
    result.info_log_level = rocksdb::InfoLogLevel::ERROR_LEVEL;
  }

  if (_enableStatistics) {
    result.statistics = rocksdb::CreateDBStatistics();
    // result.stats_dump_period_sec = 1;
  }

  result.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(getTableOptions()));

  result.create_if_missing = true;
  result.create_missing_column_families = true;

  if (_limitOpenFilesAtStartup) {
    result.max_open_files = 16;
    result.skip_stats_update_on_db_open = true;
    result.avoid_flush_during_recovery = true;
  } else {
    result.max_open_files = -1;
  }

  if (_totalWriteBufferSize > 0) {
    result.db_write_buffer_size = _totalWriteBufferSize;
  }

  // WAL_ttl_seconds needs to be bigger than the sync interval of the count
  // manager. Should be several times bigger counter_sync_seconds
  result.WAL_ttl_seconds = 60 * 60 * 24 * 30;  // we manage WAL file deletion
  // ourselves, don't let RocksDB
  // garbage collect them
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

  if (_blockCacheSize > 0) {
    result.block_cache = rocksdb::NewLRUCache(
        _blockCacheSize, static_cast<int>(_blockCacheShardBits),
        /*strict_capacity_limit*/ _enforceBlockCacheSizeLimit);
    // result.cache_index_and_filter_blocks =
    // result.pin_l0_filter_and_index_blocks_in_cache
    // _compactionReadaheadSize > 0;
  } else {
    result.no_block_cache = true;
  }
  result.cache_index_and_filter_blocks = _cacheIndexAndFilterBlocks;
  result.cache_index_and_filter_blocks_with_high_priority =
      _cacheIndexAndFilterBlocksWithHighPriority;
  result.pin_l0_filter_and_index_blocks_in_cache =
      _pinl0FilterAndIndexBlocksInCache;
  result.pin_top_level_index_and_filter = _pinTopLevelIndexAndFilter;

  result.block_size = _tableBlockSize;
  result.filter_policy.reset(rocksdb::NewBloomFilterPolicy(10, true));
  result.enable_index_compression = _enableIndexCompression;
  result.format_version = _formatVersion;
  result.prepopulate_block_cache =
      _prepopulateBlockCache
          ? rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kFlushOnly
          : rocksdb::BlockBasedTableOptions::PrepopulateBlockCache::kDisable;
  result.reserve_table_builder_memory = _reserveTableBuilderMemory;
  result.reserve_table_reader_memory = _reserveTableReaderMemory;
  result.block_align = _blockAlignDataBlocks;

  if (_checksumType == "crc32c") {
    result.checksum = rocksdb::ChecksumType::kCRC32c;
  } else if (_checksumType == "xxHash") {
    result.checksum = rocksdb::ChecksumType::kxxHash;
  } else if (_checksumType == "xxHash64") {
    result.checksum = rocksdb::ChecksumType::kxxHash64;
  } else if (_checksumType == "XXH3") {
    result.checksum = rocksdb::ChecksumType::kXXH3;
  } else {
    TRI_ASSERT(false);
    LOG_TOPIC("8d602", WARN, arangodb::Logger::FIXME)
        << "unexpected value for '--rocksdb.checksum-type'";
  }

  return result;
}

rocksdb::ColumnFamilyOptions RocksDBOptionFeature::getColumnFamilyOptions(
    RocksDBColumnFamilyManager::Family family) const {
  rocksdb::ColumnFamilyOptions result =
      RocksDBOptionsProvider::getColumnFamilyOptions(family);

  // override
  std::size_t index = static_cast<
      std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(family);
  TRI_ASSERT(index < _maxWriteBufferNumberCf.size());
  if (_maxWriteBufferNumberCf[index] > 0) {
    result.max_write_buffer_number =
        static_cast<int>(_maxWriteBufferNumberCf[index]);
  }
  if (!_minWriteBufferNumberToMergeTouched) {
    result.min_write_buffer_number_to_merge =
        static_cast<int>(defaultMinWriteBufferNumberToMerge(
            _totalWriteBufferSize, _writeBufferSize,
            result.max_write_buffer_number));
  }

  return result;
}
