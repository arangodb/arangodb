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

#include "RocksDBEngine/RocksDBEngineOptionsProvider.h"

#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb {

using namespace arangodb::options;

namespace {
constexpr uint64_t minSyncInterval = 5;
}

void RocksDBEngineOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> opts, RocksDBEngineOptions& options) {
  opts->addObsoleteOption("--server.storage-engine", "The storage engine type",
                          true);
  opts->addSection("rocksdb", "RocksDB engine");

  opts->addOption(
          "--rocksdb.minimum-disk-free-percent",
          "The minimum percentage of free disk space for considering the "
          "server healthy in health checks (0 = disable the check).",
          new DoubleParameter(&options.requiredDiskFreePercentage, /*base*/ 1.0,
                              /*minValue*/ 0.0, /*maxValue*/ 1.0),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  opts->addOption("--rocksdb.minimum-disk-free-bytes",
                  "The minimum number of free disk bytes for considering the "
                  "server healthy in health checks (0 = disable the check).",
                  new UInt64Parameter(&options.requiredDiskFreeBytes),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(30800);

  opts->addOption("--rocksdb.max-transaction-size",
                  "The transaction size limit (in bytes).",
                  new UInt64Parameter(&options.maxTransactionSize))
      .setLongDescription(R"(Transactions store all keys and values in RAM, so
large transactions run the risk of causing out-of-memory situations. This
setting allows you to ensure that it does not happen by limiting the size of
any individual transaction. Transactions whose operations would consume more
RAM than this threshold value are aborted automatically with error 32
("resource limit exceeded").)");

  opts->addOption("--rocksdb.intermediate-commit-size",
                  "An intermediate commit is performed automatically "
                  "when a transaction has accumulated operations of this "
                  "size (in bytes), and a new transaction is started.",
                  new UInt64Parameter(&options.intermediateCommitSize));

  opts->addOption("--rocksdb.intermediate-commit-count",
                  "An intermediate commit is performed automatically "
                  "when this number of operations is reached in a "
                  "transaction, and a new transaction is started.",
                  new UInt64Parameter(&options.intermediateCommitCount));

  opts->addOption("--rocksdb.max-parallel-compactions",
                  "The maximum number of parallel compactions jobs.",
                  new UInt64Parameter(&options.maxParallelCompactions));

  opts->addOption(
          "--rocksdb.sync-interval",
          "The interval for automatic, non-requested disk syncs (in "
          "milliseconds, 0 = turn automatic syncing off)",
          new UInt64Parameter(&options.syncInterval),
          arangodb::options::makeFlags(arangodb::options::Flags::OnDBServer,
                                       arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Automatic synchronization of data from RocksDB's
write-ahead logs to disk is only performed for not-yet synchronized data, and
only for operations that have been executed without the `waitForSync`
attribute.)");

  opts->addOption(
      "--rocksdb.sync-delay-threshold",
      "The threshold for self-observation of WAL disk syncs "
      "(in milliseconds, 0 = no warnings). Any WAL disk sync longer ago "
      "than this threshold triggers a warning ",
      new UInt64Parameter(&options.syncDelayThreshold),
      arangodb::options::makeFlags(
          arangodb::options::Flags::DefaultNoComponents,
          arangodb::options::Flags::OnDBServer,
          arangodb::options::Flags::OnSingle,
          arangodb::options::Flags::Uncommon));

  opts->addOption("--rocksdb.wal-file-timeout",
                  "The timeout after which unused WAL files are deleted "
                  "(in seconds).",
                  new DoubleParameter(&options.pruneWaitTime),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(Data of ongoing transactions is stored in RAM.
Transactions that get too big (in terms of number of operations involved or the
total size of data created or modified by the transaction) are committed
automatically. Effectively, this means that big user transactions are split into
multiple smaller RocksDB transactions that are committed individually.
The entire user transaction does not necessarily have ACID properties in this
case.)");

  opts->addOption("--rocksdb.wal-file-timeout-initial",
                  "The initial timeout (in seconds) after which unused WAL "
                  "files deletion kicks in after server start.",
                  new DoubleParameter(&options.pruneWaitTimeInitial),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If you decrease the value, the server starts the
removal of obsolete WAL files earlier after server start. This is useful in
testing environments that are space-restricted and do not require keeping much
WAL file data at all.)");

  opts->addOption("--rocksdb.throttle", "Enable write-throttling.",
                  new BooleanParameter(&options.useThrottle),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setLongDescription(R"(If enabled, dynamically throttles the ingest rate
of writes if necessary to reduce chances of compactions getting too far behind
and blocking incoming writes.)");

  opts->addOption("--rocksdb.throttle-slots",
                  "The number of historic metrics to use for throttle value "
                  "calculation.",
                  new UInt64Parameter(&options.throttleSlots, /*base*/ 1,
                                      /*minValue*/ 1),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(If throttling is enabled, this parameter controls
the number of previous intervals to use for throttle value calculation.)");

  opts->addOption(
          "--rocksdb.throttle-frequency",
          "The frequency for write-throttle calculations (in milliseconds).",
          new UInt64Parameter(&options.throttleFrequency),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(If the throttling is enabled, it recalculates a
new maximum ingestion rate with this frequency.)");

  opts->addOption(
          "--rocksdb.throttle-scaling-factor",
          "The adaptiveness scaling factor for write-throttle calculations.",
          new UInt64Parameter(&options.throttleScalingFactor),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(There is normally no need to change this value.)");

  opts->addOption("--rocksdb.throttle-max-write-rate",
                  "The maximum write rate enforced by throttle (in bytes per "
                  "second, 0 = unlimited).",
                  new UInt64Parameter(&options.throttleMaxWriteRate),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(The actual write rate established by the
throttling is the minimum of this value and the value that the regular throttle
calculation produces, i.e. this option can be used to set a fixed upper bound
on the write rate.)");

  opts->addOption("--rocksdb.throttle-slow-down-writes-trigger",
                  "The number of level 0 files whose payload "
                  "is not considered in throttle calculations when penalizing "
                  "the presence of L0 files.",
                  new UInt64Parameter(&options.throttleSlowdownWritesTrigger),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805)
      .setLongDescription(R"(There is normally no need to change this value.)");

  opts->addOption("--rocksdb.throttle-lower-bound-bps",
                  "The lower bound for throttle's write bandwidth "
                  "(in bytes per second).",
                  new UInt64Parameter(&options.throttleLowerBoundBps),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(30805);

#ifdef USE_ENTERPRISE
  opts->addOption("--rocksdb.create-sha-files",
                  "Whether to enable the generation of sha256 files for "
                  "each .sst file.",
                  new BooleanParameter(&options.createShaFiles),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Enterprise));
#endif

  opts->addObsoleteOption(
      "--rocksdb.use-range-delete-in-wal",
      "Enable range delete markers in the write-ahead log (WAL).", false);

  opts->addOption("--rocksdb.debug-logging",
                  "Whether to enable RocksDB debug logging.",
                  new BooleanParameter(&options.debugLogging),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(If set to `true`, enables verbose logging of
RocksDB's actions into the logfile written by ArangoDB (if the
`--rocksdb.use-file-logging` option is off), or RocksDB's own log (if the
`--rocksdb.use-file-logging` option is on).

This option is turned off by default, but you can enable it for debugging
RocksDB internals and performance.)");

  opts->addObsoleteOption("--rocksdb.edge-cache",
                          "Whether to use the in-memory cache for edges",
                          false);

  opts->addOption("--rocksdb.verify-sst",
                  "Verify the validity of .sst files present in the "
                  "`engine-rocksdb` directory on startup.",
                  new BooleanParameter(&options.verifySst),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::Command,
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnAgent,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31100)
      .setLongDescription(R"(If set to `true`, during startup, all .sst files
in the `engine-rocksdb` folder in the database directory are checked for
potential corruption and errors. The server process stops after the check and
returns an exit code of `0` if the validation was successful, or a non-zero
exit code if there is an error in any of the .sst files.)");

  opts->addOption("--rocksdb.wal-archive-size-limit",
                  "The maximum total size (in bytes) of archived WAL files to "
                  "keep on the leader (0 = unlimited).",
                  new UInt64Parameter(&options.maxWalArchiveSizeLimit),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(31200)
      .setLongDescription(R"(A value of `0` does not restrict the size of the
archive, so the leader removes archived WAL files when there are no replication
clients needing them. Any non-zero value restricts the size of the WAL files
archive to about the specified value and trigger WAL archive file deletion once
the threshold is reached. You can use this to get rid of archived WAL files in
a disk size-constrained environment.

**Note**: The value is only a threshold, so the archive may get bigger than
the configured value until the background thread actually deletes files from
the archive. Also note that deletion from the archive only kicks in after
`--rocksdb.wal-file-timeout-initial` seconds have elapsed after server start.

Archived WAL files are normally deleted automatically after a short while when
there is no follower attached that may read from the archive. However, in case
when there are followers attached that may read from the archive, WAL files
normally remain in the archive until their contents have been streamed to the
followers. In case there are slow followers that cannot catch up, this causes a
growth of the WAL files archive over time.

You can use the option to force a deletion of WAL files from the archive even if
there are followers attached that may want to read the archive. In case the
option is set and a leader deletes files from the archive that followers want to
read, this aborts the replication on the followers. Followers can restart the
replication doing a resync, though, but they may not be able to catch up if WAL
file deletion happens too early.

Thus it is best to leave this option at its default value of `0` except in cases
when disk size is very constrained and no replication is used.)");

  opts->addOption("--rocksdb.auto-flush-min-live-wal-files",
                  "The minimum number of live WAL files that triggers an "
                  "auto-flush of WAL "
                  "and column family data.",
                  new UInt64Parameter(&options.autoFlushMinWalFiles),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31005);

  opts->addOption(
          "--rocksdb.auto-flush-check-interval",
          "The interval (in seconds) in which auto-flushes of WAL and column "
          "family data is executed.",
          new DoubleParameter(&options.autoFlushCheckInterval),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31005);

  opts->addOption(
          "--rocksdb.force-legacy-comparator",
          "If set to `true`, forces a new database directory to use the "
          "legacy sorting method. This is only for testing. Don't use.",
          new BooleanParameter(&options.forceLegacySortingMethod),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31202);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  opts->addOption(
          "--rocksdb.force-legacy-little-endian-keys",
          "Force usage of legacy little endian key encoding when creating "
          "a new RocksDB database directory. DO NOT USE IN PRODUCTION.",
          new BooleanParameter(&options.forceLittleEndianKeys),
          arangodb::options::makeFlags(
              arangodb::options::Flags::DefaultNoComponents,
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::Experimental,
              arangodb::options::Flags::OnAgent,
              arangodb::options::Flags::OnDBServer,
              arangodb::options::Flags::OnSingle))
      .setIntroducedIn(31200)
      .setLongDescription(R"(If enabled and a new RocksDB database
is generated, the legacy little endian key encoding is used.

Only use this option for testing purposes! It is bad for performance and
disables a few features like parallel index generation!)");
#endif

  // TODO: consider moving this option to --rocksdb.export-read-write-metrics
  opts->addOption("--server.export-read-write-metrics",
                  "Whether to enable metrics for document reads and writes.",
                  new BooleanParameter(&options.exportReadWriteMetrics),
                  arangodb::options::makeFlags(
                      arangodb::options::Flags::DefaultNoComponents,
                      arangodb::options::Flags::OnDBServer,
                      arangodb::options::Flags::OnSingle,
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(Enabling this option exposes the following
additional metrics via the `GET /_admin/metrics/v2` endpoint:

- `arangodb_document_writes_total`
- `arangodb_document_writes_replication_total`
- `arangodb_document_insert_time`
- `arangodb_document_read_time`
- `arangodb_document_update_time`
- `arangodb_document_replace_time`
- `arangodb_document_remove_time`
- `arangodb_collection_truncates_total`
- `arangodb_collection_truncates_replication_total`
- `arangodb_collection_truncate_time`
)");
}

void RocksDBEngineOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> opts, RocksDBEngineOptions& options) {
  if (options.throttleScalingFactor == 0) {
    options.throttleScalingFactor = 1;
  }

  if (options.throttleSlots < 8) {
    options.throttleSlots = 8;
  }

  if (options.syncInterval > 0) {
    if (options.syncInterval < minSyncInterval) {
      LOG_TOPIC("bbd68", FATAL, arangodb::Logger::CONFIG)
          << "invalid value for --rocksdb.sync-interval. Please use a value "
          << "of at least " << minSyncInterval;
      FATAL_ERROR_EXIT();
    }

    if (options.syncDelayThreshold > 0 &&
        options.syncDelayThreshold <= options.syncInterval) {
      if (!opts->processingResult().touched("rocksdb.sync-interval") &&
          opts->processingResult().touched("rocksdb.sync-delay-threshold")) {
        LOG_TOPIC("c3f45", WARN, arangodb::Logger::CONFIG)
            << "invalid value for --rocksdb.sync-delay-threshold. should be "
               "higher "
            << "than the value of --rocksdb.sync-interval ("
            << options.syncInterval << ")";
      }

      options.syncDelayThreshold = 10 * options.syncInterval;
      LOG_TOPIC("c0fa3", WARN, arangodb::Logger::CONFIG)
          << "auto-adjusting value of --rocksdb.sync-delay-threshold to "
          << options.syncDelayThreshold << " ms";
    }
  }

  if (options.pruneWaitTimeInitial < 10) {
    LOG_TOPIC("a9667", WARN, arangodb::Logger::ENGINES)
        << "consider increasing the value for "
           "--rocksdb.wal-file-timeout-initial. "
        << "Replication clients might have trouble to get in sync";
  }
}

}  // namespace arangodb
