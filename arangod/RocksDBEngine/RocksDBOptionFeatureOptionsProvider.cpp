////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBEngine/RocksDBOptionFeatureOptionsProvider.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_set>

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Option.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RocksDBEngine/RocksDBColumnFamilyManager.h"

namespace arangodb {

namespace {

std::unordered_set<std::string> const compressionTypes = {
    std::string{kCompressionTypeSnappy}, std::string{kCompressionTypeLZ4},
    std::string{kCompressionTypeLZ4HC}, std::string{kCompressionTypeNone}};

std::unordered_set<std::string> const blockCacheTypes = {
    std::string{kBlockCacheTypeLRU}, std::string{kBlockCacheTypeHyperClock}};

std::unordered_set<std::string> const checksumTypes = {
    std::string{kChecksumTypeCRC32C}, std::string{kChecksumTypeXXHash},
    std::string{kChecksumTypeXXHash64}, std::string{kChecksumTypeXXH3}};

std::unordered_set<std::string> const compactionStyles = {
    std::string{kCompactionStyleLevel}, std::string{kCompactionStyleUniversal},
    std::string{kCompactionStyleFifo}, std::string{kCompactionStyleNone}};

}  // namespace

using namespace arangodb::options;

void RocksDBOptionFeatureOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> opts,
    RocksDBOptionFeatureOptions& options) {
  opts->addSection("rocksdb", "RocksDB engine");

  opts->addOption("--rocksdb.wal-directory",
                  "Absolute path for RocksDB WAL files. If not set, a "
                  "subdirectory `journals` inside the database directory "
                  "is used.",
                  new StringParameter(&options.walDirectory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.target-file-size-base",
                  "Per-file target file size for compaction (in bytes). The "
                  "actual target file size for each level is "
                  "`--rocksdb.target-file-size-base` multiplied by "
                  "`--rocksdb.target-file-size-multiplier` ^ (level - 1)",
                  new UInt64Parameter(&options.targetFileSizeBase),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.target-file-size-multiplier",
      "The multiplier for `--rocksdb.target-file-size`. A value of 1 means "
      "that files in different levels will have the same size.",
      new UInt64Parameter(&options.targetFileSizeMultiplier, /*base*/ 1,
                          /*minValue*/ 1),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  TRI_ASSERT(compressionTypes.contains(options.compressionType));
  opts->addOption("--rocksdb.compression-type",
                  "The compression algorithm to use within RocksDB.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.compressionType, compressionTypes))
      .setIntroducedIn(31000);

  opts->addOption("--rocksdb.transaction-lock-stripes",
                  "The number of lock stripes to use for transaction locks.",
                  new UInt64Parameter(&options.transactionLockStripes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30902)
      .setLongDescription(R"(You can control the number of lock stripes to use
for RocksDB's transaction lock manager with this option. You can use higher
values to reduce a potential contention in the lock manager.

The option defaults to the number of available cores, but is increased to a
value of `16` if the number of cores is lower.)");

  opts->addOption(
          "--rocksdb.transaction-lock-timeout",
          "If positive, specifies the wait timeout in milliseconds when "
          " a transaction attempts to lock a document. A negative value "
          "is not recommended as it can lead to deadlocks (0 = no waiting, < 0 "
          "no timeout). This is deprecated since we internally control the "
          "lock timeout for different cases.",
          new Int64Parameter(&options.transactionLockTimeout),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setDeprecatedIn(31206);

  opts->addOption(
          "--rocksdb.total-write-buffer-size",
          "The maximum total size of in-memory write buffers (0 = unbounded).",
          new UInt64Parameter(&options.totalWriteBufferSize),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Dynamic,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The total amount of data to build up in all
in-memory buffers (backed by log files). You can use this option together with
the block cache size configuration option to limit memory usage.

If set to `0`, the memory usage is not limited.

If set to a value larger than `0`, this caps memory usage for write buffers but
may have an effect on performance. If there is more than 4 GiB of RAM in the
system, the default value is `(system RAM size - 2 GiB) * 0.5`.

For systems with less RAM, the default values are:

- 512 MiB for systems with between 1 and 4 GiB of RAM.
- 256 MiB for systems with less than 1 GiB of RAM.)");

  opts->addOption("--rocksdb.write-buffer-size",
                  "The amount of data to build up in memory before "
                  "converting to a sorted on-disk file (0 = disabled).",
                  new UInt64Parameter(&options.writeBufferSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The amount of data to build up in each in-memory
buffer (backed by a log file) before closing the buffer and queuing it to be
flushed to standard storage. Larger values than the default may improve
performance, especially for bulk loads.)");

  opts->addOption(
          "--rocksdb.max-write-buffer-number",
          "The maximum number of write buffers that build up in memory "
          "(default: number of column families + 2 = 12 write buffers). "
          "You can only increase the number.",
          new UInt64Parameter(&options.maxWriteBufferNumber),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If this number is reached before the buffers can
be flushed, writes are slowed or stalled.)");

  opts->addOption(
          "--rocksdb.max-write-buffer-size-to-maintain",
          "The maximum size of immutable write buffers that build up in memory "
          "per column family. Larger values mean that more in-memory data "
          "can be used for transaction conflict checking (-1 = use automatic "
          "default value, 0 = do not keep immutable flushed write buffers, "
          "which is the default and usually correct).",
          new Int64Parameter(&options.maxWriteBufferSizeToMaintain),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The default value `0` restores the memory usage
pattern of version 3.6. This makes RocksDB not keep any flushed immutable
write-buffers in memory.)");

  opts->addOption("--rocksdb.max-total-wal-size",
                  "The maximum total size of WAL files that force a flush "
                  "of stale column families.",
                  new UInt64Parameter(&options.maxTotalWalSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(When reached, force a flush of all column families
whose data is backed by the oldest WAL files. If you set this option to a low
value, regular flushing of column family data from memtables is triggered, so
that WAL files can be moved to the archive.

If you set this option to a high value, regular flushing is avoided but may
prevent WAL files from being moved to the archive and being removed.)");

  opts->addOption(
      "--rocksdb.delayed-write-rate",
      "Limit the write rate to the database (in bytes per second) when writing "
      "to the last mem-table allowed and if more than 3 mem-tables are "
      "allowed, or if a certain number of level-0 files are surpassed and "
      "writes need to be slowed down.",
      new UInt64Parameter(&options.delayedWriteRate),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.min-write-buffer-number-to-merge",
                  "The minimum number of write buffers that are merged "
                  "together before writing to storage.",
                  new UInt64Parameter(&options.minWriteBufferNumberToMerge),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.num-levels",
                  "The number of levels for the database in the LSM tree.",
                  new UInt64Parameter(&options.numLevels, /*base*/ 1,
                                      /*minValue*/ 1, /*maxValue*/ 20),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.num-uncompressed-levels",
                  "The number of levels that do not use compression in the "
                  "LSM tree.",
                  new UInt64Parameter(&options.numUncompressedLevels),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Levels above the default of `2` use
compression to reduce the disk space requirements for storing data in these
levels.)");

  opts->addOption("--rocksdb.dynamic-level-bytes",
                  "Whether to determine the number of bytes for each level "
                  "dynamically to minimize space amplification.",
                  new BooleanParameter(&options.dynamicLevelBytes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, the amount of data in each level
of the LSM tree is determined dynamically to minimize the space amplification.
Otherwise, the level sizes are fixed. The dynamic sizing allows RocksDB to
maintain a well-structured LSM tree regardless of total data size.)");

  opts->addOption("--rocksdb.max-bytes-for-level-base",
                  "If not using dynamic level sizes, this controls the "
                  "maximum total data size for level-1 of the LSM tree.",
                  new UInt64Parameter(&options.maxBytesForLevelBase),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.max-bytes-for-level-multiplier",
      "If not using dynamic level sizes, the maximum number of "
      "bytes for level L of the LSM tree can be calculated as "
      " max-bytes-for-level-base * "
      "(max-bytes-for-level-multiplier ^ (L-1))",
      new DoubleParameter(&options.maxBytesForLevelMultiplier, /*base*/ 1.0,
                          /*minValue*/ 0.0,
                          /*maxValue*/ std::numeric_limits<double>::max(),
                          /*minInclusive*/ false),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption(
          "--rocksdb.block-align-data-blocks",
          "If enabled, data blocks are aligned on the lesser of page size and "
          "block size.",
          new BooleanParameter(&options.blockAlignDataBlocks),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(This may waste some memory but may reduce the
number of cross-page I/O operations.)");

  opts->addOption("--rocksdb.enable-pipelined-write",
                  "If enabled, use a two stage write queue for WAL writes "
                  "and memtable writes.",
                  new BooleanParameter(&options.enablePipelinedWrite),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.enable-statistics",
                  "Whether RocksDB statistics should be enabled.",
                  new BooleanParameter(&options.enableStatistics),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.optimize-filters-for-hits",
      "Whether the implementation should optimize the filters mainly for cases "
      "where keys are found rather than also optimize for keys missed. You can "
      "enable the option if you know that there are very few misses or the "
      "performance in the case of misses is not important for your "
      "application.",
      new BooleanParameter(&options.optimizeFiltersForHits),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.use-direct-reads", "Use O_DIRECT for reading files.",
      new BooleanParameter(&options.useDirectReads),
      arangodb::options::makeFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--rocksdb.use-direct-io-for-flush-and-compaction",
      "Use O_DIRECT for writing files for flush and compaction.",
      new BooleanParameter(&options.useDirectIoForFlushAndCompaction),
      arangodb::options::makeFlags(arangodb::options::Flags::Uncommon));

  opts->addOption(
      "--rocksdb.use-fsync",
      "Whether to use fsync calls when writing to disk (set to false "
      "for issuing fdatasync calls only).",
      new BooleanParameter(&options.useFSync),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.max-background-jobs",
                  "The maximum number of concurrent background jobs "
                  "(compactions and flushes).",
                  new Int32Parameter(&options.maxBackgroundJobs),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The jobs are submitted to the low priority thread
pool. The default value is the number of processors in the system.)");

  opts->addOption("--rocksdb.max-subcompactions",
                  "The maximum number of concurrent sub-jobs for a "
                  "background compaction.",
                  new UInt32Parameter(&options.maxSubcompactions),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.level0-compaction-trigger",
                  "The number of level-0 files that triggers a compaction.",
                  new Int64Parameter(&options.level0CompactionTrigger),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Compaction of level-0 to level-1 is triggered when
this many files exist in level-0. If you set this option to a higher number, it
may help bulk writes at the expense of slowing down reads.)");

  opts->addOption("--rocksdb.level0-slowdown-trigger",
                  "The number of level-0 files that triggers a write slowdown",
                  new Int64Parameter(&options.level0SlowdownTrigger),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(When this many files accumulate in level-0, writes
are slowed down to `--rocksdb.delayed-write-rate` to allow compaction to
catch up.)");

  opts->addOption("--rocksdb.level0-stop-trigger",
                  "The number of level-0 files that triggers a full write stop",
                  new Int64Parameter(&options.level0StopTrigger),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(When this many files accumulate in level-0, writes
are stopped to allow compaction to catch up.)");

  opts->addOption(
          "--rocksdb.pending-compactions-slowdown-trigger",
          "The number of pending compaction bytes that triggers a "
          "write slowdown.",
          new UInt64Parameter(&options.pendingCompactionBytesSlowdownTrigger),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805);

  opts->addOption(
          "--rocksdb.pending-compactions-stop-trigger",
          "The number of pending compaction bytes that triggers a full "
          "write stop.",
          new UInt64Parameter(&options.pendingCompactionBytesStopTrigger),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805);

  opts->addOption(
          "--rocksdb.num-threads-priority-high",
          "The number of threads for high priority operations (e.g. flush).",
          new UInt32Parameter(&options.numThreadsHigh, /*base*/ 1,
                              /*minValue*/ 0,
                              /*maxValue*/ 64),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30805)
      .setLongDescription(R"(The recommended value is to set this equal to
`max-background-flushes`. The default value is `number of processors / 2`.)");

  opts->addOption("--rocksdb.block-cache-estimated-entry-charge",
                  "The estimated charge of cache entries (in bytes) for the "
                  "hyper-clock cache.",
                  new UInt64Parameter(&options.blockCacheEstimatedEntryCharge),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206);

  TRI_ASSERT(blockCacheTypes.contains(options.blockCacheType));
  opts->addOption("--rocksdb.block-cache-type",
                  "The block cache type to use (note: the 'hyper-clock' cache "
                  "type is experimental).",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.blockCacheType, blockCacheTypes))
      .setIntroducedIn(31206);

  opts->addOption("--rocksdb.num-threads-priority-low",
                  "The number of threads for low priority operations (e.g. "
                  "compaction).",
                  new UInt32Parameter(&options.numThreadsLow, /*base*/ 1,
                                      /*minValue*/ 0,
                                      /*maxValue*/ 256),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The default value is
`number of processors / 2`.)");

  opts->addOption("--rocksdb.block-cache-size",
                  "The size of block cache (in bytes).",
                  new UInt64Parameter(&options.blockCacheSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Dynamic,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(This is the maximum size of the block cache in
bytes. Increasing this value may improve performance. If there is more than
4 GiB of RAM in the system, the default value is
`(system RAM size - 2GiB) * 0.3`.

For systems with less RAM, the default values are:

- 512 MiB for systems with between 2 and 4 GiB of RAM.
- 256 MiB for systems with between 1 and 2 GiB of RAM.
- 128 MiB for systems with less than 1 GiB of RAM.)");

  opts->addOption("--rocksdb.block-cache-shard-bits",
                  "The number of shard bits to use for the block cache "
                  "(-1 = default value).",
                  new Int64Parameter(&options.blockCacheShardBits, /*base*/ 1,
                                     /*minValue*/ -1,
                                     /*maxValue*/ 20, /*minInclusive*/ true,
                                     /*maxInclusive*/ false),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(The number of bits used to shard the block cache
to allow concurrent operations. To keep individual shards at a reasonable size
(i.e. at least 512 KiB), keep this value to at most
`block-cache-shard-bits / 512 KiB`. Default: `block-cache-size / 2^19`.)");

  opts->addOption("--rocksdb.enforce-block-cache-size-limit",
                  "If enabled, strictly enforces the block cache size limit.",
                  new BooleanParameter(&options.enforceBlockCacheSizeLimit),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Whether the maximum size of the RocksDB block
cache is strictly enforced. You can set this option to limit the memory usage of
the block cache to at most the specified size. If inserting a data block into
the cache would exceed the cache's capacity, the data block is not inserted.
If disabled, a data block may still get inserted into the cache. It is evicted
later, but the cache may temporarily grow beyond its capacity limit.

The default value for `--rocksdb.enforce-block-cache-size-limit` was `false`
before version 3.10, but was changed to `true` from version 3.10 onwards.

To improve stability of memory usage and prevent exceeding the block cache
capacity limit (as configurable via `--rocksdb.block-cache-size`), it is
recommended to set this option to `true`.)");

  opts->addOption("--rocksdb.cache-index-and-filter-blocks",
                  "If enabled, the RocksDB block cache quota also includes "
                  "RocksDB memtable sizes.",
                  new BooleanParameter(&options.cacheIndexAndFilterBlocks),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If you set this option to `true`, RocksDB tracks
all loaded index and filter blocks in the block cache, so that they count
towards RocksDB's block cache memory limit.

If you set this option to `false`, the memory usage for index and filter blocks
is not accounted for.

The default value of `--rocksdb.cache-index-and-filter-blocks` was `false` in
versions before 3.10, and was changed to `true` from version 3.10 onwards.

To improve stability of memory usage and avoid untracked memory allocations by
RocksDB, it is recommended to set this option to `true`. Note that tracking
index and filter blocks leaves less room for other data in the block cache, so
in case servers have unused RAM capacity available, it may be useful to increase
the overall size of the block cache.)");

  opts->addOption(
      "--rocksdb.cache-index-and-filter-blocks-with-high-priority",
      "If enabled and `--rocksdb.cache-index-and-filter-blocks` is also "
      "enabled, cache index and filter blocks with high priority, "
      "making index and filter blocks be less likely to be evicted than "
      "data blocks.",
      new BooleanParameter(&options.cacheIndexAndFilterBlocksWithHighPriority),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.pin-l0-filter-and-index-blocks-in-cache",
      "If enabled and `--rocksdb.cache-index-and-filter-blocks` is also "
      "enabled, filter and index blocks are pinned and only evicted from "
      "cache when the table reader is freed.",
      new BooleanParameter(&options.pinl0FilterAndIndexBlocksInCache),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption(
      "--rocksdb.pin-top-level-index-and-filter",
      "If enabled and `--rocksdb.cache-index-and-filter-blocks` is also "
      "enabled, the top-level index of partitioned filter and index blocks "
      "are pinned and only evicted from cache when the table reader is "
      "freed.",
      new BooleanParameter(&options.pinTopLevelIndexAndFilter),
      arangodb::options::makeFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.table-block-size",
                  "The approximate size (in bytes) of the user data packed "
                  "per block for uncompressed data.",
                  new UInt64Parameter(&options.tableBlockSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.recycle-log-file-num",
                  "If enabled, keep a pool of log files around for recycling.",
                  new SizeTParameter(&options.recycleLogFileNum),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption(
          "--rocksdb.bloom-filter-bits-per-key",
          "The average number of bits to use per key in a Bloom filter.",
          new DoubleParameter(&options.bloomBitsPerKey),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31003);

  opts->addOption(
      "--rocksdb.compaction-read-ahead-size",
      "If non-zero, bigger reads are performed when doing compaction. If you "
      "run RocksDB on spinning disks, you should set this to at least 2 MB. "
      "That way, RocksDB's compaction does sequential instead of random reads.",
      new UInt64Parameter(&options.compactionReadaheadSize),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnAgent,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.use-file-logging",
                  "Use a file-base logger for RocksDB's own logs.",
                  new BooleanParameter(&options.useFileLogging),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If set to `true`, enables writing of RocksDB's own
informational log files into RocksDB's database directory.

This option is turned off by default, but you can enable it for debugging
RocksDB internals and performance.)");

  opts->addOption("--rocksdb.wal-recovery-skip-corrupted",
                  "Skip corrupted records in WAL recovery.",
                  new BooleanParameter(&options.skipCorrupted),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.limit-open-files-at-startup",
                  "Limit the amount of .sst files RocksDB inspects at "
                  "startup, in order to reduce the startup I/O operations.",
                  new BooleanParameter(&options.limitOpenFilesAtStartup),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle));

  opts->addOption("--rocksdb.allow-fallocate",
                  "Whether to allow RocksDB to use fallocate calls. "
                  "If disabled, fallocate calls are bypassed and no "
                  "pre-allocation is done.",
                  new BooleanParameter(&options.allowFAllocate),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Preallocation is turned on by default, but you can
turn it off for operating system versions that are known to have issues with it.
This option only has an effect on operating systems that support
`fallocate`.)");

  TRI_ASSERT(checksumTypes.contains(options.checksumType));
  opts->addOption("--rocksdb.checksum-type",
                  "The checksum type to use for table files.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.checksumType, checksumTypes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  TRI_ASSERT(compactionStyles.contains(options.compactionStyle));
  opts->addOption(
          "--rocksdb.compaction-style",
          "The compaction style which is used to pick the next file(s) to "
          "be compacted (note: all styles except 'level' are experimental).",
          new DiscreteValuesParameter<StringParameter>(&options.compactionStyle,
                                                       compactionStyles),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  std::unordered_set<uint32_t> formatVersions = {3, 4, 5, 6};
  opts->addOption("--rocksdb.format-version",
                  "The table format version to use inside RocksDB.",
                  new DiscreteValuesParameter<UInt32Parameter>(
                      &options.formatVersion, formatVersions),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000)
      .setLongDescription(
          R"(Note that format version 6 can only be read by RocksDB
versions >= 8.6.0. Thus switching to format version 6 will make the database
files incompatible with ArangoDB versions with a lower RocksDB version in case
of downgrading.)");

  opts->addOption("--rocksdb.optimize-filters-for-memory",
                  "Optimize RocksDB bloom filters to reduce internal memory "
                  "fragmentation.",
                  new BooleanParameter(&options.optimizeFiltersForMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206);

  opts->addOption("--rocksdb.enable-index-compression",
                  "Enable index compression.",
                  new BooleanParameter(&options.enableIndexCompression),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  opts->addOption("--rocksdb.enable-blob-files",
                  "Enable blob files for the documents column family.",
                  new BooleanParameter(&options.enableBlobFiles),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption(
          "--rocksdb.enable-blob-cache",
          "Enable caching of blobs in the block cache for the documents "
          "column family.",
          new BooleanParameter(&options.enableBlobCache),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206);

  opts->addOption("--rocksdb.min-blob-size",
                  "The size threshold for storing documents in blob files (in "
                  "bytes, 0 = store all documents in blob files). "
                  "Requires `--rocks.enable-blob-files`.",
                  new UInt64Parameter(&options.minBlobSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption("--rocksdb.blob-file-size",
                  "The size limit for blob files in the documents column "
                  "family (in bytes). Requires `--rocksdb.enable-blob-files`.",
                  new UInt64Parameter(&options.blobFileSize),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption("--rocksdb.blob-file-starting-level",
                  "The level from which on to use blob files in the documents "
                  "column family. Requires `--rocksdb.enable-blob-files`.",
                  new UInt32Parameter(&options.blobFileStartingLevel),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206);

  opts->addOption(
          "--rocksdb.blob-garbage-collection-age-cutoff",
          "The age cutoff for garbage collecting blob files in the documents "
          "column family (percentage value from 0 to 1 determines how many "
          "blob files are garbage collected during compaction). Requires "
          "`--rocksdb.enable-blob-files` and "
          "`--rocksdb.enable-blob-garbage-collection`.",
          new DoubleParameter(&options.blobGarbageCollectionAgeCutoff, 1.0, 0.0,
                              1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption(
          "--rocksdb.blob-garbage-collection-force-threshold",
          "The garbage ratio threshold for scheduling targeted compactions "
          "for the oldest blob files in the documents column family "
          "(percentage value between 0 and 1). "
          "Requires `--rocksdb.enable-blob-files` and "
          "`--rocksdb.enable-blob-garbage-collection`.",
          new DoubleParameter(&options.blobGarbageCollectionForceThreshold, 1.0,
                              0.0, 1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  TRI_ASSERT(compressionTypes.contains(options.blobCompressionType));
  opts->addOption("--rocksdb.blob-compression-type",
                  "The compression algorithm to use for blob data in the "
                  "documents column family. "
                  "Requires `--rocksdb.enable-blob-files`.",
                  new DiscreteValuesParameter<StringParameter>(
                      &options.blobCompressionType, compressionTypes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption(
          "--rocksdb.enable-blob-garbage-collection",
          "Enable blob garbage collection during compaction in the "
          "documents column family. Requires `--rocksdb.enable-blob-files`.",
          new BooleanParameter(&options.enableBlobGarbageCollection),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption("--rocksdb.prepopulate-blob-cache",
                  "Pre-populate the blob cache on flushes.",
                  new BooleanParameter(&options.prepopulateBlobCache),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31206);

  opts->addOption(
          "--rocksdb.block-cache-jemalloc-allocator",
          "Use jemalloc-based memory allocator for RocksDB block cache.",
          new BooleanParameter(&options.useJemallocAllocator),
          arangodb::options::makeFlags(arangodb::options::Flags::Experimental,
                                       arangodb::options::Flags::Uncommon,
                                       arangodb::options::Flags::OnAgent,
                                       arangodb::options::Flags::OnDBServer,
                                       arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100)
      .setLongDescription(
          R"(The jemalloc-based memory allocator for the RocksDB block cache
will also exclude the block cache contents from coredumps, potentially making
generated coredumps a lot smaller.
In order to use this option, the executable needs to be compiled with jemalloc
support (which is the default on Linux).)");

  opts->addOption("--rocksdb.prepopulate-block-cache",
                  "Pre-populate block cache on flushes.",
                  new BooleanParameter(&options.prepopulateBlockCache),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  opts->addOption("--rocksdb.reserve-table-builder-memory",
                  "Account for table building memory in block cache.",
                  new BooleanParameter(&options.reserveTableBuilderMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  opts->addOption("--rocksdb.reserve-table-reader-memory",
                  "Account for table reader memory in block cache.",
                  new BooleanParameter(&options.reserveTableReaderMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31000);

  opts->addOption("--rocksdb.reserve-file-metadata-memory",
                  "account for .sst file metadata memory in block cache",
                  new BooleanParameter(&options.reserveFileMetadataMemory),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31100);

  opts->addOption("--rocksdb.periodic-compaction-ttl",
                  "Time-to-live (in seconds) for periodic compaction of .sst "
                  "files, based on the file age (0 = no periodic compaction).",
                  new UInt64Parameter(&options.periodicCompactionTtl),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30903)
      .setLongDescription(R"(The default value from RocksDB is ~30 days. To
avoid periodic auto-compaction and the I/O caused by it, you can set this
option to `0`.)");

  opts->addOption("--rocksdb.partition-files-for-documents",
                  "If enabled, the document data for different "
                  "collections/shards will end up in "
                  "different .sst files.",
                  new BooleanParameter(&options.partitionFilesForDocumentsCf),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Enabling this option will make RocksDB's
compaction write the document data for different collections/shards
into different .sst files. Otherwise the document data from different
collections/shards can be mixed and written into the same .sst files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different collections/shards
are written to in parallel.
The disavantage of enabling this option is that there can be more .sst
files than when the option is turned off, and the disk space used by
these .sst files can be higher than if there are fewer .sst files (this
is because there is some per-.sst file overhead).
In particular on deployments with many collections/shards
this can lead to a very high number of .sst files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. Thus the option should only be enabled on deployments with a
limited number of collections/shards.)");

  opts->addOption(
          "--rocksdb.partition-files-for-primary-index",
          "If enabled, the primary index data for different "
          "collections/shards will end up in "
          "different .sst files.",
          new BooleanParameter(&options.partitionFilesForPrimaryIndexCf),
          arangodb::options::makeFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Enabling this option will make RocksDB's
compaction write the primary index data for different collections/shards
into different .sst files. Otherwise the primary index data from different
collections/shards can be mixed and written into the same .sst files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different collections/shards
are written to in parallel.
The disavantage of enabling this option is that there can be more .sst
files than when the option is turned off, and the disk space used by
these .sst files can be higher than if there are fewer .sst files (this
is because there is some per-.sst file overhead).
In particular on deployments with many collections/shards
this can lead to a very high number of .sst files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. Thus the option should only be enabled on deployments with a
limited number of collections/shards.)");

  opts->addOption("--rocksdb.partition-files-for-edge-index",
                  "If enabled, the index data for different edge "
                  "indexes will end up in different .sst files.",
                  new BooleanParameter(&options.partitionFilesForEdgeIndexCf),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Enabling this option will make RocksDB's
compaction write the edge index data for different edge collections/shards
into different .sst files. Otherwise the edge index data from different
edge collections/shards can be mixed and written into the same .sst files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different edge collections/shards
are written to in parallel.
The disavantage of enabling this option is that there can be more .sst
files than when the option is turned off, and the disk space used by
these .sst files can be higher than if there are fewer .sst files (this
is because there is some per-.sst file overhead).
In particular on deployments with many edge collections/shards
this can lead to a very high number of .sst files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. Thus the option should only be enabled on deployments with a
limited number of edge collections/shards.)");

  opts->addOption("--rocksdb.partition-files-for-persistent-index",
                  "If enabled, the index data for different persistent "
                  "indexes will end up in different .sst files.",
                  new BooleanParameter(&options.partitionFilesForVPackIndexCf),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Enabling this option will make RocksDB's
compaction write the persistent index data for different persistent
indexes (also indexes from different collections/shards) into different
.sst files. Otherwise the persistent index data from different
collections/shards/indexes can be mixed and written into the same .sst files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different collections/shards/indexes
are written to in parallel.
The disavantage of enabling this option is that there can be more .sst
files than when the option is turned off, and the disk space used by
these .sst files can be higher than if there are fewer .sst files (this
is because there is some per-.sst file overhead).
In particular on deployments with many collections/shards/indexes
this can lead to a very high number of .sst files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. Thus the option should only be enabled on deployments with a
limited number of edge collections/shards/indexes.)");

  opts->addOption("--rocksdb.partition-files-for-mdi-index",
                  "If enabled, the index data for different mdi "
                  "indexes will end up in different .sst files.",
                  new BooleanParameter(&options.partitionFilesForMdiIndexCf),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(Enabling this option makes RocksDB's
compaction write the persistent index data for different `mdi`
indexes (also indexes from different collections/shards) into different
`.sst` files. Otherwise the persistent index data from different
collections/shards/indexes can be mixed and written into the same `.sst` files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different collections/shards/indexes
are written to in parallel.
The disadvantage of enabling this option is that there can be more `.sst`
files than when the option is turned off, and the disk space used by
these `.sst` files can be higher than if there are fewer `.sst` files (this
is because there is some per-`.sst` file overhead).

In particular on deployments with many collections/shards/indexes
this can lead to a very high number of `.sst` files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. Thus the option should only be enabled on deployments with a
limited number of edge collections/shards/indexes.)");

  opts->addOption("--rocksdb.partition-files-for-vector-index",
                  "If enabled, the index data for different vector "
                  "indexes will end up in different .sst files.",
                  new BooleanParameter(&options.partitionFilesForVectorIndexCf),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31204)
      .setLongDescription(R"(Enabling this option makes RocksDB's
compaction write the index data for different vector
indexes (also indexes from different collections/shards) into different
.sst files. Otherwise, the index data from different
collections/shards/indexes can be mixed and written into the same .sst files.

Enabling this option usually has the benefit of making the RocksDB
compaction more efficient when a lot of different collections/shards/indexes
are written to in parallel.
The disadvantage of enabling this option is that there can be more .sst
files than when the option is disabled, and the disk space used by
these .sst files can be higher than if there are fewer .sst files
because there is some overhead per .sst file.
For deployments with many collections/shards/indexes in particular,
this can lead to a very high number of .sst files, with the potential
of outgrowing the maximum number of file descriptors the ArangoDB process
can open. The option should thus only be enabled for deployments with a
limited number of edge collections/shards/indexes.)");

  options.ioUringEnabled = _ioUringEnabled;
  opts->addOption(
          "--rocksdb.use-io_uring",
          "Check for existence of io_uring at startup and use it if available. "
          "Should be set to false only to opt out of using io_uring.",
          new BooleanParameter(&options.ioUringEnabled),
          arangodb::options::makeFlags(arangodb::options::Flags::Uncommon,
                                       arangodb::options::Flags::OnAgent,
                                       arangodb::options::Flags::OnDBServer,
                                       arangodb::options::Flags::OnSingle))
      .setIntroducedIn(3'12'00);

  //////////////////////////////////////////////////////////////////////////////
  /// add column family-specific options now
  //////////////////////////////////////////////////////////////////////////////
  static constexpr std::initializer_list<RocksDBColumnFamilyManager::Family>
      families{RocksDBColumnFamilyManager::Family::Definitions,
               RocksDBColumnFamilyManager::Family::Documents,
               RocksDBColumnFamilyManager::Family::PrimaryIndex,
               RocksDBColumnFamilyManager::Family::EdgeIndex,
               RocksDBColumnFamilyManager::Family::VPackIndex,
               RocksDBColumnFamilyManager::Family::GeoIndex,
               RocksDBColumnFamilyManager::Family::FulltextIndex,
               RocksDBColumnFamilyManager::Family::ReplicatedLogs,
               RocksDBColumnFamilyManager::Family::MdiIndex,
               RocksDBColumnFamilyManager::Family::MdiVPackIndex,
               RocksDBColumnFamilyManager::Family::VectorIndex};

  auto addMaxWriteBufferNumberCf =
      [&options, opts](RocksDBColumnFamilyManager::Family family) {
        std::string name = RocksDBColumnFamilyManager::name(
            family, RocksDBColumnFamilyManager::NameMode::External);
        std::size_t index = static_cast<
            std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(
            family);
        auto introducedIn = 30800;
        if (family == RocksDBColumnFamilyManager::Family::MdiVPackIndex ||
            family == RocksDBColumnFamilyManager::Family::MdiIndex) {
          introducedIn = 31200;
        }
        if (family == RocksDBColumnFamilyManager::Family::VectorIndex) {
          introducedIn = 31204;
        }

        opts->addOption(
                "--rocksdb.max-write-buffer-number-" + name,
                "If non-zero, overrides the value of "
                "`--rocksdb.max-write-buffer-number` for the " +
                    name + " column family",
                new UInt64Parameter(&options.maxWriteBufferNumberCf[index]),
                arangodb::options::makeDefaultFlags(
                    arangodb::options::Flags::Uncommon))
            .setIntroducedIn(introducedIn);
      };
  for (auto family : families) {
    addMaxWriteBufferNumberCf(family);
  }
}

void RocksDBOptionFeatureOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> opts,
    RocksDBOptionFeatureOptions& options) {
  if (options.writeBufferSize > 0 && options.writeBufferSize < 1024 * 1024) {
    LOG_TOPIC("4ce44", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--rocksdb.write-buffer-size'";
    FATAL_ERROR_EXIT();
  }
  if (options.totalWriteBufferSize > 0 &&
      options.totalWriteBufferSize < 64 * 1024 * 1024) {
    LOG_TOPIC("4ab88", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--rocksdb.total-write-buffer-size'";
    FATAL_ERROR_EXIT();
  }
  if (options.maxBackgroundJobs != -1 && options.maxBackgroundJobs < 1) {
    LOG_TOPIC("cfc5a", FATAL, arangodb::Logger::STARTUP)
        << "invalid value for '--rocksdb.max-background-jobs'";
    FATAL_ERROR_EXIT();
  }

  options.minWriteBufferNumberToMergeTouched = opts->processingResult().touched(
      "--rocksdb.min-write-buffer-number-to-merge");

  if (options.blockCacheType == kBlockCacheTypeLRU &&
      opts->processingResult().touched(
          "--rocksdb.block-cache-estimated-entry-charge")) {
    LOG_TOPIC("a527b", WARN, arangodb::Logger::ENGINES)
        << "Setting value of '--rocksdb.block-cache-estimated-entry-charge' "
           "has no effect when using LRU block cache";
  }

  if (options.enforceBlockCacheSizeLimit &&
      !opts->processingResult().touched("--rocksdb.block-cache-shard-bits")) {
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
    options.blockCacheShardBits = std::clamp(
        int64_t(
            std::floor(std::log2(static_cast<double>(options.blockCacheSize) /
                                 kMinBlockCacheShardSize))),
        int64_t(1), int64_t(10));

    // TODO: hyper clock cache probably doesn't need as many shards. check this.
  }

#ifndef ARANGODB_HAVE_JEMALLOC
  // on some platforms, jemalloc is not available, because it is not compiled
  // in by default. to make the startup of the server not fail in such
  // environment, turn off the option automatically
  if (options.useJemallocAllocator) {
    options.useJemallocAllocator = false;
    LOG_TOPIC("b3164", INFO, Logger::STARTUP)
        << "disabling jemalloc allocator for RocksDB - jemalloc not compiled";
  }
#endif

  if (!options.enableBlobFiles) {
    // turn off blob garbage collection to avoid potential side effects
    // for performance
    options.enableBlobGarbageCollection = false;
  }
}

}  // namespace arangodb
