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

#include "Dump/DumpOptionsProvider.h"

#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Basics/files.h"
#include "Basics/StringUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <algorithm>

namespace {

/// @brief minimum amount of data to fetch from server in a single batch
constexpr uint64_t minChunkSize = 1024 * 128;

/// @brief maximum amount of data to fetch from server in a single batch
constexpr uint64_t maxChunkSize = 1024 * 1024 * 96;

/// @brief minimum number of documents per batch
constexpr uint64_t minDocsPerBatch = 100;

/// @brief maximum number of documents per batch
constexpr uint64_t maxDocsPerBatch = 100 * 1000;

}  // namespace

namespace arangodb {

using namespace arangodb::options;

void DumpOptionsProvider::declareOptions(
    std::shared_ptr<ProgramOptions> options, DumpFeatureOptions& opts) {
  options->addOption(
      "--collection",
      "Restrict the dump to this collection name (can be specified multiple "
      "times). Either --collection or --ignore-collection can be used at the "
      "same time.",
      new VectorParameter<StringParameter>(&opts.collections));

  options
      ->addOption(
          "--ignore-collection",
          "Ignore and exclude this collection during the dump process (can be "
          "specified multiple times). Either --collection or "
          "--ignore-collection can be used at the same time. ",
          new VectorParameter<StringParameter>(&opts.collectionsToBeIgnored))
      .setIntroducedIn(31200);

  options
      ->addOption(
          "--shard",
          "Restrict the dump to this shard (can be specified multiple times).",
          new VectorParameter<StringParameter>(&opts.shards))
      .setIntroducedIn(30800);

  options->addOption("--initial-batch-size",
                     "The initial size for individual data batches (in bytes).",
                     new UInt64Parameter(&opts.initialChunkSize));

  options->addOption("--batch-size",
                     "The maximum size for individual data batches (in bytes).",
                     new UInt64Parameter(&opts.maxChunkSize));

  options
      ->addOption("--docs-per-batch",
                  "The maximum number of documents to be returned per batch.",
                  new UInt64Parameter(&opts.docsPerBatch))
      .setIntroducedIn(31200);

  options->addOption(
      "--threads",
      "The maximum number of collections/shards to process in parallel.",
      new UInt32Parameter(&opts.threadCount),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options->addOption("--dump-data", "Whether to dump collection data.",
                     new BooleanParameter(&opts.dumpData));

  options
      ->addOption("--dump-views", "Whether to dump view definitions.",
                  new BooleanParameter(&opts.dumpViews))
      .setIntroducedIn(31100);

  options->addOption("--all-databases", "Whether to dump all databases.",
                     new BooleanParameter(&opts.allDatabases));

  options->addOption(
      "--force",
      "Continue dumping even in the face of some server-side errors.",
      new BooleanParameter(&opts.force));

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "Continue dumping even if a sharding prototype collection is "
      "not backed up, too.",
      new BooleanParameter(&opts.ignoreDistributeShardsLikeErrors));

  options->addOption("--include-system-collections",
                     "Include system collections.",
                     new BooleanParameter(&opts.includeSystemCollections));

  options->addOption("--output-directory",
                     "The folder path to write the dump to.",
                     new StringParameter(&opts.outputPath));

  options->addOption("--overwrite", "Overwrite data in the output directory.",
                     new BooleanParameter(&opts.overwrite));

  options->addOption("--progress", "Show the dump progress.",
                     new BooleanParameter(&opts.progress));

  options->addObsoleteOption(
      "--envelope",
      "Wrap each document into a {type, data} envelope "
      "(this is required for compatibility with v3.7 and before).",
      false);

  options->addObsoleteOption("--tick-start",
                             "Only include data after this tick.", true);

  options->addObsoleteOption("--tick-end",
                             "Last tick to be included in data dump.", true);

  options->addOption("--maskings", "A path to a file with masking definitions.",
                     new StringParameter(&opts.maskingsFile));

  options->addOption("--compress-output",
                     "Compress files containing collection contents using the "
                     "gzip format.",
                     new BooleanParameter(&opts.useGzipForStorage));

  options
      ->addOption("--dump-vpack",
                  "Dump collection data in velocypack format (more compact "
                  "than JSON, but requires ArangoDB 3.12 or higher to restore)",
                  new BooleanParameter(&opts.useVPack),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Experimental,
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31200);

  options
      ->addOption("--parallel-dump", "Enable highly parallel dump behavior.",
                  new BooleanParameter(&opts.useParalleDump),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setLongDescription(R"(This option enables a highly parallel variant
of the dump protocol on the server side. It is only supported with ArangoDB
servers running version 3.12 or higher.
If the dump should be restored into versions of ArangoDB older than 3.12, this
option should be turned off.)")
      .setIntroducedIn(31008)
      .setIntroducedIn(31102);
  // option was renamed in 3.12
  options->addOldOption("--use-experimental-dump", "--parallel-dump");

  options
      ->addOption(
          "--split-files",
          "Split a collection in multiple files to increase throughput.",
          new BooleanParameter(&opts.splitFiles))
      .setLongDescription(R"(This option only has effect when the option
`--parallel-dump` is set to `true`. Restoring split files also
requires an arangorestore version that is capable of restoring data of a
single collection/shard from multiple files.)")
      .setIntroducedIn(31010)
      .setIntroducedIn(31102);

  options
      ->addOption("--dbserver-worker-threads",
                  "Number of worker threads on each DB-Server.",
                  new UInt64Parameter(&opts.dbserverWorkerThreads),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31008)
      .setIntroducedIn(31102);

  options
      ->addOption("--dbserver-prefetch-batches",
                  "Number of batches to prefetch on each DB-Server.",
                  new UInt64Parameter(&opts.dbserverPrefetchBatches),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31008)
      .setIntroducedIn(31102);

  options
      ->addOption("--local-writer-threads", "Number of local writer threads.",
                  new UInt64Parameter(&opts.localWriterThreads),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31008)
      .setIntroducedIn(31102);

  options
      ->addOption("--local-network-threads",
                  "Number of local network threads, i.e. how many requests "
                  "are sent in parallel.",
                  new UInt64Parameter(&opts.localNetworkThreads, /*base*/ 1,
                                      /*minValue*/ 1),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31008)
      .setIntroducedIn(31102);
}

void DumpOptionsProvider::validateOptions(
    std::shared_ptr<ProgramOptions> options, DumpFeatureOptions& opts) {
  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    opts.outputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("a62e0", FATAL, arangodb::Logger::DUMP)
        << "expecting at most one directory, got " +
               arangodb::basics::StringUtils::join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  // clamp chunk values to allowed ranges
  opts.docsPerBatch =
      std::clamp(opts.docsPerBatch, ::minDocsPerBatch, ::maxDocsPerBatch);
  opts.initialChunkSize =
      std::clamp(opts.initialChunkSize, ::minChunkSize, ::maxChunkSize);
  opts.maxChunkSize =
      std::clamp(opts.maxChunkSize, opts.initialChunkSize, ::maxChunkSize);

  if (options->processingResult().touched("server.database") &&
      opts.allDatabases) {
    LOG_TOPIC("17e2b", FATAL, arangodb::Logger::DUMP)
        << "cannot use --server.database and --all-databases at the same time";
    FATAL_ERROR_EXIT();
  }

  if (options->processingResult().touched("collection") &&
      options->processingResult().touched("ignore-collection")) {
    LOG_TOPIC("17e2a", FATAL, arangodb::Logger::DUMP)
        << "cannot use --collection and --ignore-collection at the same time";
    FATAL_ERROR_EXIT();
  }

  // trim trailing slash from path because it may cause problems on ...
  // Windows
  if (!opts.outputPath.empty() &&
      opts.outputPath.back() == TRI_DIR_SEPARATOR_CHAR) {
    TRI_ASSERT(opts.outputPath.size() > 0);
    opts.outputPath.pop_back();
  }
  TRI_NormalizePath(opts.outputPath);

  uint32_t clamped =
      std::clamp(opts.threadCount, uint32_t(1),
                 4 * static_cast<uint32_t>(NumberOfCores::getValue()));
  if (opts.threadCount != clamped) {
    LOG_TOPIC("0460e", WARN, Logger::DUMP)
        << "capping --threads value to " << clamped;
    opts.threadCount = clamped;
  }

  if (opts.splitFiles && !opts.useParalleDump) {
    LOG_TOPIC("b0cbe", FATAL, Logger::DUMP)
        << "--split-files is only available when using "
           "--parallel-dump.";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
