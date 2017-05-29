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
#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/DatabasePathFeature.h"

#include <rocksdb/options.h>
#include <rocksdb/table.h>

#include <thread>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::options;

namespace {
rocksdb::Options rocksDBDefaults;
rocksdb::BlockBasedTableOptions rocksDBTableOptionsDefaults;
}

RocksDBOptionFeature::RocksDBOptionFeature(
    application_features::ApplicationServer* server)
    : application_features::ApplicationFeature(server, "RocksDBOption"),
      _writeBufferSize(rocksDBDefaults.write_buffer_size),
      _maxWriteBufferNumber(rocksDBDefaults.max_write_buffer_number),
      _delayedWriteRate(rocksDBDefaults.delayed_write_rate),
      _minWriteBufferNumberToMerge(
          rocksDBDefaults.min_write_buffer_number_to_merge),
      _numLevels(rocksDBDefaults.num_levels),
      _numUncompressedLevels(2),
      _maxBytesForLevelBase(rocksDBDefaults.max_bytes_for_level_base),
      _maxBytesForLevelMultiplier(
          rocksDBDefaults.max_bytes_for_level_multiplier),
      _baseBackgroundCompactions(rocksDBDefaults.base_background_compactions),
      _maxBackgroundCompactions(rocksDBDefaults.max_background_compactions),
      _maxFlushes(rocksDBDefaults.max_background_flushes),
      _numThreadsHigh(rocksDBDefaults.max_background_flushes),
      _numThreadsLow(rocksDBDefaults.max_background_compactions),
      _blockCacheSize((TRI_PhysicalMemory >= (static_cast<uint64_t>(4) << 30))
        ? static_cast<uint64_t>(((TRI_PhysicalMemory - (static_cast<uint64_t>(2) << 30)) * 0.3))
        : (256 << 20)),
      _blockCacheShardBits(0),
      _tableBlockSize(rocksDBTableOptionsDefaults.block_size),
      _recycleLogFileNum(rocksDBDefaults.recycle_log_file_num),
      _compactionReadaheadSize(rocksDBDefaults.compaction_readahead_size),
      _verifyChecksumsInCompaction(
          rocksDBDefaults.verify_checksums_in_compaction),
      _optimizeFiltersForHits(rocksDBDefaults.optimize_filters_for_hits),
      _useDirectReads(rocksDBDefaults.use_direct_reads),
      _useDirectWrites(rocksDBDefaults.use_direct_writes),
      _useFSync(rocksDBDefaults.use_fsync),
      _skipCorrupted(false) {
  uint64_t testSize = _blockCacheSize >> 19;
  while (testSize > 0) {
    _blockCacheShardBits++;
    testSize >>= 1;
  }

  setOptional(true);
  requiresElevatedPrivileges(false);
  startsAfter("DatabasePath");

  // increase parallelism and re-fetch the number of threads
  rocksDBDefaults.IncreaseParallelism(std::thread::hardware_concurrency());
  _numThreadsHigh = rocksDBDefaults.max_background_flushes;
  _numThreadsLow = rocksDBDefaults.max_background_compactions;
}

void RocksDBOptionFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addSection("rocksdb", "Configure the RocksDB engine");

  options->addObsoleteOption("--rocksdb.enabled",
                             "obsolete always active - Whether or not the "
                             "RocksDB engine is enabled for the persistent "
                             "index",
                             true);

  options->addOption("--rocksdb.write-buffer-size",
                     "amount of data to build up in memory before converting "
                     "to a sorted on-disk file (0 = disabled)",
                     new UInt64Parameter(&_writeBufferSize));

  options->addOption("--rocksdb.max-write-buffer-number",
                     "maximum number of write buffers that built up in memory",
                     new UInt64Parameter(&_maxWriteBufferNumber));

  options->addHiddenOption(
      "--rocksdb.delayed_write_rate",
      "limited write rate to DB (in bytes per second) if we are writing to the "
      "last "
      "mem table allowed and we allow more than 3 mem tables",
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

  options->addHiddenOption("--rocksdb.max-bytes-for-level-base",
                           "control maximum total data size for level-1",
                           new UInt64Parameter(&_maxBytesForLevelBase));

  options->addOption("--rocksdb.max-bytes-for-level-multiplier",
                     "maximum number of bytes for level L can be calculated as "
                     "max-bytes-for-level-base * "
                     "(max-bytes-for-level-multiplier ^ (L-1))",
                     new DoubleParameter(&_maxBytesForLevelMultiplier));

  options->addHiddenOption(
      "--rocksdb.verify-checksums-in-compaction",
      "if true, compaction will verify checksum on every read that happens "
      "as part of compaction",
      new BooleanParameter(&_verifyChecksumsInCompaction));

  options->addHiddenOption(
      "--rocksdb.optimize-filters-for-hits",
      "this flag specifies that the implementation should optimize the filters "
      "mainly for cases where keys are found rather than also optimize for "
      "keys "
      "missed. This would be used in cases where the application knows that "
      "there are very few misses or the performance in the case of misses is "
      "not "
      "important",
      new BooleanParameter(&_optimizeFiltersForHits));

#ifdef __linux__
  options->addHiddenOption("--rocksdb.use-direct-reads",
                           "use O_DIRECT for reading files",
                           new BooleanParameter(&_useDirectReads));

  options->addHiddenOption("--rocksdb.use-direct-writes",
                           "use O_DIRECT for writing files",
                           new BooleanParameter(&_useDirectWrites));
#endif

  options->addHiddenOption("--rocksdb.use-fsync",
                           "issue an fsync when writing to disk (set to true "
                           "for issuing fdatasync only)",
                           new BooleanParameter(&_useFSync));

  options->addHiddenOption(
      "--rocksdb.base-background-compactions",
      "suggested number of concurrent background compaction jobs",
      new UInt64Parameter(&_baseBackgroundCompactions));

  options->addOption("--rocksdb.max-background-compactions",
                     "maximum number of concurrent background compaction jobs",
                     new UInt64Parameter(&_maxBackgroundCompactions));

  options->addOption("--rocksdb.max-background-flushes",
                     "maximum number of concurrent flush operations",
                     new UInt64Parameter(&_maxFlushes));

  options->addOption(
      "--rocksdb.num-threads-priority-high",
      "number of threads for high priority operations (e.g. flush)",
      new UInt64Parameter(&_numThreadsHigh));

  options->addOption(
      "--rocksdb.num-threads-priority-low",
      "number of threads for low priority operations (e.g. compaction)",
      new UInt64Parameter(&_numThreadsLow));

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

  options->addHiddenOption(
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
  if (_baseBackgroundCompactions < 1 || _baseBackgroundCompactions > 64) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.base-background-compactions'";
    FATAL_ERROR_EXIT();
  }
  if (_maxBackgroundCompactions < _baseBackgroundCompactions) {
    _maxBackgroundCompactions = _baseBackgroundCompactions;
  }
  if (_maxFlushes < 1 || _maxFlushes > 64) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.max-background-flushes'";
    FATAL_ERROR_EXIT();
  }
  if (_numThreadsHigh < 1 || _numThreadsHigh > 64) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-high'";
    FATAL_ERROR_EXIT();
  }
  if (_numThreadsLow < 1 || _numThreadsLow > 256) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.num-threads-priority-low'";
    FATAL_ERROR_EXIT();
  }
  if (_blockCacheShardBits > 32) {
    LOG_TOPIC(FATAL, arangodb::Logger::FIXME)
        << "invalid value for '--rocksdb.block-cache-shard-bits'";
    FATAL_ERROR_EXIT();
  }
}

void RocksDBOptionFeature::start() {
  LOG_TOPIC(TRACE, Logger::FIXME) << "using RocksDB options:"
                                  << " write_buffer_size: " << _writeBufferSize
                                  << " max_write_buffer_number: " << _maxWriteBufferNumber
                                  << " delayed_write_rate: " << _delayedWriteRate
                                  << " min_write_buffer_number_to_merge: " << _minWriteBufferNumberToMerge
                                  << " num_levels: " << _numLevels
                                  << " max_bytes_for_level_base: " << _maxBytesForLevelBase
                                  << " max_bytes_for_level_multiplier: " << _maxBytesForLevelMultiplier
                                  << " base_background_compactions: " << _baseBackgroundCompactions
                                  << " max_background_compactions: " << _maxBackgroundCompactions
                                  << " max_flushes: " << _maxFlushes
                                  << " num_threads_high: " << _numThreadsHigh
                                  << " num_threads_low: " << _numThreadsLow
                                  << " block_cache_size: " << _blockCacheSize
                                  << " block_cache_shard_bits: " << _blockCacheShardBits
                                  << " compaction_read_ahead_size: " << _compactionReadaheadSize
                                  << " verify_checksums_in_compaction: " << std::boolalpha << _verifyChecksumsInCompaction
                                  << " optimize_filters_for_hits: " << std::boolalpha << _optimizeFiltersForHits
                                  << " use_direct_reads: " << std::boolalpha << _useDirectReads
                                  << " use_direct_writes: " << std::boolalpha << _useDirectWrites
                                  << " use_fsync: " << std::boolalpha << _useFSync;
}
