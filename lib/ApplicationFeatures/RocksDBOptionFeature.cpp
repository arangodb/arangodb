////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBOptionFeature.h"
#include "Basics/Exceptions.h"
#include "Basics/FileUtils.h"
#include "Basics/process-utils.h"
#include "Basics/tri-strings.h"
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"

#include <rocksdb/options.h>
#include <rocksdb/table.h>
#include <rocksdb/utilities/transaction_db.h>

#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
rocksdb::TransactionDBOptions rocksDBTrxDefaults;
rocksdb::Options rocksDBDefaults;
rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;
}

RocksDBOptionFeature::RocksDBOptionFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "RocksDBOption"),
      _transactionLockTimeout(rocksDBTrxDefaults.transaction_lock_timeout),
      _writeBufferSize(rocksDBDefaults.write_buffer_size),
      _maxWriteBufferNumber(rocksDBDefaults.max_write_buffer_number),
      _maxTotalWalSize(80 << 20),
      _delayedWriteRate(rocksDBDefaults.delayed_write_rate),
      _minWriteBufferNumberToMerge(
          rocksDBDefaults.min_write_buffer_number_to_merge),
      _numLevels(rocksDBDefaults.num_levels),
      _numUncompressedLevels(2),
      _maxBytesForLevelBase(rocksDBDefaults.max_bytes_for_level_base),
      _maxBytesForLevelMultiplier(
          rocksDBDefaults.max_bytes_for_level_multiplier),
      _maxBackgroundJobs(rocksDBDefaults.max_background_jobs),
      _maxSubcompactions(rocksDBDefaults.max_subcompactions),
      _numThreadsHigh(0),
      _numThreadsLow(0),
      _blockCacheSize((TRI_PhysicalMemory >= (static_cast<uint64_t>(4) << 30))
        ? static_cast<uint64_t>(((TRI_PhysicalMemory - (static_cast<uint64_t>(2) << 30)) * 0.3))
        : (256 << 20)),
      _blockCacheShardBits(0),
      _tableBlockSize(std::max(rocksDBTableOptionsDefaults.block_size, static_cast<decltype(rocksDBTableOptionsDefaults.block_size)>(16 * 1024))),
      _recycleLogFileNum(rocksDBDefaults.recycle_log_file_num),
      _compactionReadaheadSize(2 * 1024 * 1024),//rocksDBDefaults.compaction_readahead_size
      _level0CompactionTrigger(2),
      _level0SlowdownTrigger(rocksDBDefaults.level0_slowdown_writes_trigger),
      _level0StopTrigger(rocksDBDefaults.level0_stop_writes_trigger),
      _enablePipelinedWrite(rocksDBDefaults.enable_pipelined_write),
      _optimizeFiltersForHits(rocksDBDefaults.optimize_filters_for_hits),
      _useDirectReads(rocksDBDefaults.use_direct_reads),
      _useDirectIoForFlushAndCompaction(rocksDBDefaults.use_direct_io_for_flush_and_compaction),
      _useFSync(rocksDBDefaults.use_fsync),
      _skipCorrupted(false),
      _dynamicLevelBytes(true),
      _enableStatistics(false) {
  uint64_t testSize = _blockCacheSize >> 19;
  while (testSize > 0) {
    _blockCacheShardBits++;
    testSize >>= 1;
  }
  // setting the number of background jobs to
  _maxBackgroundJobs = static_cast<int32_t>(std::max((size_t)2,
                                std::min(TRI_numberProcessors(), (size_t)8)));
  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("Daemon");
  startsAfter("DatabasePath");
}

void RocksDBOptionFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");

  options->addObsoleteOption("--rocksdb.enabled",
                             "obsolete always active - Whether or not the "
                             "RocksDB engine is enabled for the persistent "
                             "index",
                             true);
  
  options->addOption("--rocksdb.wal-directory",
                     "optional path to the RocksDB WAL directory. "
                     "If not set, the WAL directory will be located inside the regular data directory",
                     new StringParameter(&_walDirectory));
  
  options->addOption("--rocksdb.transaction-lock-timeout",
                     "If positive, specifies the wait timeout in milliseconds when "
                     " a transaction attempts to lock a document. Defaults is 1000. A negative value "
                     "is not recommended as it can lead to deadlocks (0 = no waiting, < 0 no timeout)",
                     new Int64Parameter(&_transactionLockTimeout));

  options->addOption("--rocksdb.write-buffer-size",
                     "amount of data to build up in memory before converting "
                     "to a sorted on-disk file (0 = disabled)",
                     new UInt64Parameter(&_writeBufferSize));

  options->addOption("--rocksdb.max-write-buffer-number",
                     "maximum number of write buffers that built up in memory",
                     new UInt64Parameter(&_maxWriteBufferNumber));
  
  options->addOption("--rocksdb.max-total-wal-size",
                     "maximum total size of WAL files that will force flush stale column families",
                     new UInt64Parameter(&_maxTotalWalSize));

  options->addHiddenOption(
      "--rocksdb.delayed_write_rate",
      "limited write rate to DB (in bytes per second) if we are writing to the "
      "last mem-table allowed and we allow more than 3 mem-tables, or if we "
      "have surpassed a certain number of level-0 files and need to slowdown "
      "writes",
      new UInt64Parameter(&_delayedWriteRate));

  options->addOption("--rocksdb.min-write-buffer-number-to-merge",
                     "minimum number of write buffers that will be merged "
                     "together before writing "
                     "to storage",
                     new UInt64Parameter(&_minWriteBufferNumberToMerge));

  options->addOption("--rocksdb.num-levels",
                     "number of levels for the database",
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

  options->addOption("--rocksdb.enable-pipelined-write",
                     "if true, use a two stage write queue for WAL writes and memtable writes",
                     new BooleanParameter(&_enablePipelinedWrite));
  
  options->addOption("--rocksdb.enable-statistics",
                     "whether or not RocksDB statistics should be turned on",
                     new BooleanParameter(&_enableStatistics));

  options->addHiddenOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for "
      "keys missed. This would be used in cases where the application knows "
      "that there are very few misses or the performance in the case of "
      "misses is not important",
      new BooleanParameter(&_optimizeFiltersForHits));

#ifdef __linux__
  options->addHiddenOption("--rocksdb.use-direct-reads",
                           "use O_DIRECT for reading files",
                           new BooleanParameter(&_useDirectReads));

  options->addHiddenOption("--rocksdb.use-direct-io-for-flush-and-compaction",
                           "use O_DIRECT for flush and compaction",
                           new BooleanParameter(&_useDirectIoForFlushAndCompaction));
#endif

  options->addHiddenOption("--rocksdb.use-fsync",
                           "issue an fsync when writing to disk (set to true "
                           "for issuing fdatasync only)",
                           new BooleanParameter(&_useFSync));

  options->addHiddenOption(
      "--rocksdb.max-background-jobs",
      "Maximum number of concurrent background jobs (compactions and flushes)",
      new Int32Parameter(&_maxBackgroundJobs));

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
                     "size of block cache in bytes",
                     new UInt64Parameter(&_blockCacheSize));

  options->addOption("--rocksdb.block-cache-shard-bits",
                     "number of shard bits to use for block cache",
                     new UInt64Parameter(&_blockCacheShardBits));

  options->addOption("--rocksdb.table-block-size",
                     "approximate size (in bytes) of user data packed per block",
                     new UInt64Parameter(&_tableBlockSize));

  options->addHiddenOption("--rocksdb.recycle-log-file-num",
                           "number of log files to keep around for recycling",
                           new UInt64Parameter(&_recycleLogFileNum));

  options->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "if non-zero, we perform bigger reads when doing compaction. If you're "
      "running RocksDB on spinning disks, you should set this to at least 2MB. "
      "that way RocksDB's compaction is doing sequential instead of random "
      "reads.",
      new UInt64Parameter(&_compactionReadaheadSize));

  options->addHiddenOption("--rocksdb.wal-recovery-skip-corrupted",
                           "skip corrupted records in WAL recovery",
                           new BooleanParameter(&_skipCorrupted));
}

void RocksDBOptionFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_writeBufferSize > 0 && _writeBufferSize < 1024 * 1024) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.write-buffer-size'";
    FATAL_ERROR_EXIT();
  }
  if (_maxBytesForLevelMultiplier <= 0.0) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-bytes-for-level-multiplier'";
    FATAL_ERROR_EXIT();
  }
  if (_numLevels < 1 || _numLevels > 20) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-levels'";
    FATAL_ERROR_EXIT();
  }

  if (_maxBackgroundJobs != -1 &&
      (_maxBackgroundJobs < 1 || _maxBackgroundJobs > 128)) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-background-jobs'";
    FATAL_ERROR_EXIT();
  }
  
  if (_numThreadsHigh > 64) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-high'";
    FATAL_ERROR_EXIT();
  }
  if ( _numThreadsLow > 256) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-low'";
    FATAL_ERROR_EXIT();
  }
  if (_maxSubcompactions > _numThreadsLow) {
    _maxSubcompactions = _numThreadsLow;
  }
  if (_blockCacheShardBits > 32) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.block-cache-shard-bits'";
    FATAL_ERROR_EXIT();
  }
}

void RocksDBOptionFeature::start() {
  uint32_t max = _maxBackgroundJobs / 2;
  uint32_t clamped = std::max(std::min((uint32_t)TRI_numberProcessors(), max), 1U);
  // lets test this out
  if (_numThreadsHigh == 0) {
    _numThreadsHigh = clamped;
  }
  if (_numThreadsLow == 0) {
    _numThreadsLow = clamped;
  }

  LOG_TOPIC(TRACE, Logger::ROCKSDB) << "using RocksDB options:"
                                    << " wal_dir: " << _walDirectory << "'"
                                    << ", write_buffer_size: " << _writeBufferSize
                                    << ", max_write_buffer_number: " << _maxWriteBufferNumber
                                    << ", max_total_wal_size: " << _maxTotalWalSize
                                    << ", delayed_write_rate: " << _delayedWriteRate
                                    << ", min_write_buffer_number_to_merge: " << _minWriteBufferNumberToMerge
                                    << ", num_levels: " << _numLevels
                                    << ", num_uncompressed_levels: " << _numUncompressedLevels
                                    << ", max_bytes_for_level_base: " << _maxBytesForLevelBase
                                    << ", max_bytes_for_level_multiplier: " << _maxBytesForLevelMultiplier
                                    << ", max_background_jobs: " << _maxBackgroundJobs
                                    << ", max_sub_compactions: " << _maxSubcompactions
                                    << ", num_threads_high: " << _numThreadsHigh
                                    << ", num_threads_low: " << _numThreadsLow
                                    << ", block_cache_size: " << _blockCacheSize
                                    << ", block_cache_shard_bits: " << _blockCacheShardBits
                                    << ", table_block_size: " << _tableBlockSize
                                    << ", recycle_log_file_num: " << _recycleLogFileNum 
                                    << ", compaction_read_ahead_size: " << _compactionReadaheadSize
                                    << ", level0_compaction_trigger: " << _level0CompactionTrigger
                                    << ", level0_slowdown_trigger: " << _level0SlowdownTrigger
                                    << ", enable_pipelined_write: " << _enablePipelinedWrite
                                    << ", optimize_filters_for_hits: " << _optimizeFiltersForHits
                                    << ", use_direct_reads: " << _useDirectReads
                                    << ", use_direct_io_for_flush_and_compaction: " << _useDirectIoForFlushAndCompaction
                                    << ", use_fsync: " << _useFSync
                                    << ", dynamic_level_bytes: " << std::boolalpha << _dynamicLevelBytes;
}
