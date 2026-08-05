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

#include "Restore/RestoreOptionsProvider.h"

#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

#include <algorithm>

using namespace arangodb::options;

namespace arangodb {

void RestoreOptionsProvider::declareOptionsImpl(
    std::shared_ptr<ProgramOptions> options, RestoreFeatureOptions& opts) {
  options->addOption(
      "--collection",
      "Restrict the restore to this collection name (can be specified multiple "
      "times).",
      new VectorParameter<StringParameter>(&opts.collections));

  options->addOption("--view",
                     "Restrict the restore to this view name (can be specified "
                     "multiple times).",
                     new VectorParameter<StringParameter>(&opts.views));

  options->addObsoleteOption(
      "--recycle-ids", "collection ids are now handled automatically", false);

  options->addOption("--batch-size",
                     "The maximum size for individual data batches (in bytes).",
                     new UInt64Parameter(&opts.chunkSize));

  options->addOption(
      "--threads", "The maximum number of collections to process in parallel.",
      new UInt32Parameter(&opts.threadCount),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));

  options
      ->addOption("--initial-connect-retries",
                  "The number of connect retries for the initial connection.",
                  new UInt32Parameter(&opts.initialConnectRetries))
      .setIntroducedIn(30713)
      .setIntroducedIn(30801);

  options->addOption("--include-system-collections",
                     "Include system collections.",
                     new BooleanParameter(&opts.includeSystemCollections));

  options->addOption("--create-database",
                     "Create the target database if it does not exist.",
                     new BooleanParameter(&opts.createDatabase));

  options
      ->addOption("--max-unused-buffers-capacity",
                  "Maximum cumulated size of spare in-memory buffers to keep.",
                  new UInt64Parameter(&opts.maxUnusedBufferSize),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon))
      .setIntroducedIn(31200)
      .setLongDescription(
          R"(Maximum cumulated size of in-memory buffers to keep around for
sending batches.
A value > 0 will increase the memory usage of arangorestore, but can help in
avoiding repeated memory allocations for building new in-memory buffers.)");

  options->addOption(
      "--force-same-database",
      "Force the same database name as in the source `dump.json` file.",
      new BooleanParameter(&opts.forceSameDatabase));

  options->addOption("--all-databases", "Restore the data of all databases.",
                     new BooleanParameter(&opts.allDatabases));

  options->addOption("--input-directory", "The input directory.",
                     new StringParameter(&opts.inputPath));

  options->addOption(
      "--cleanup-duplicate-attributes",
      "Clean up duplicate attributes (use first specified value) in input "
      "documents instead of making the restore operation fail.",
      new BooleanParameter(&opts.cleanupDuplicateAttributes),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));

  options->addOption("--import-data", "Import data into collection.",
                     new BooleanParameter(&opts.importData));

  options->addOption("--create-collection", "Create collection structure.",
                     new BooleanParameter(&opts.importStructure));

  options->addOption("--progress", "Show the progress.",
                     new BooleanParameter(&opts.progress));

  options->addOption("--overwrite", "Overwrite collections if they exist.",
                     new BooleanParameter(&opts.overwrite));

  options->addOption("--continue", "Continue the restore operation.",
                     new BooleanParameter(&opts.continueRestore));

  options->addObsoleteOption(
      "--envelope",
      "wrap each document into a {type, data} envelope "
      "(this is required for compatibility with v3.7 and before).",
      false);

  options
      ->addOption("--enable-revision-trees",
                  "Enable revision trees for new collections if the collection "
                  "attributes `syncByRevision` and "
                  "`usesRevisionsAsDocumentIds` are missing.",
                  new BooleanParameter(&opts.enableRevisionTrees))
      .setIntroducedIn(30807);

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
  options->addOption(
      "--fail-after-update-continue-file", "",
      new BooleanParameter(&opts.failOnUpdateContinueFile),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Uncommon));
#endif

  options->addOption(
      "--number-of-shards",
      "Override the `numberOfShards` value (can be specified multiple "
      "times, e.g. --number-of-shards 2 --number-of-shards "
      "myCollection=3).",
      new VectorParameter<StringParameter>(&opts.numberOfShards));

  options->addOption(
      "--replication-factor",
      "Override the `replicationFactor` value (can be specified "
      "multiple times, e.g. --replication-factor 2 "
      "--replication-factor myCollection=3).",
      new VectorParameter<StringParameter>(&opts.replicationFactor));

  options
      ->addOption("--write-concern",
                  "Override the `writeConcern` value (can be specified "
                  "multiple times, e.g. --write-concern 2 "
                  "--write-concern myCollection=3).",
                  new VectorParameter<StringParameter>(&opts.writeConcern))
      .setIntroducedIn(31200);

  options->addOption(
      "--ignore-distribute-shards-like-errors",
      "Continue the restore even if the sharding prototype collection is "
      "missing.",
      new BooleanParameter(&opts.ignoreDistributeShardsLikeErrors));

  options->addOption(
      "--force",
      "Continue the restore even in the face of some server-side errors.",
      new BooleanParameter(&opts.force));

  // deprecated options
  options
      ->addOption(
          "--default-number-of-shards",
          "The default `numberOfShards` value if not specified in the dump.",
          new UInt64Parameter(&opts.defaultNumberOfShards),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);

  options
      ->addOption(
          "--default-replication-factor",
          "The default `replicationFactor` value if not specified in the dump.",
          new UInt64Parameter(&opts.defaultReplicationFactor),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon))
      .setDeprecatedIn(30322)
      .setDeprecatedIn(30402);
}

void RestoreOptionsProvider::validateOptionsImpl(
    std::shared_ptr<ProgramOptions> options, RestoreFeatureOptions& opts) {
  using arangodb::basics::StringUtils::join;

  auto const& positionals = options->processingResult()._positionals;
  size_t n = positionals.size();

  if (1 == n) {
    opts.inputPath = positionals[0];
  } else if (1 < n) {
    LOG_TOPIC("d249a", FATAL, arangodb::Logger::RESTORE)
        << "expecting at most one directory, got " + join(positionals, ", ");
    FATAL_ERROR_EXIT();
  }

  if (opts.allDatabases) {
    if (options->processingResult().touched("server.database")) {
      LOG_TOPIC("94d22", FATAL, arangodb::Logger::RESTORE)
          << "cannot use --server.database and --all-databases at the same "
             "time";
      FATAL_ERROR_EXIT();
    }

    if (opts.forceSameDatabase) {
      LOG_TOPIC("fd66a", FATAL, arangodb::Logger::RESTORE)
          << "cannot use --force-same-database and --all-databases at the same "
             "time";
      FATAL_ERROR_EXIT();
    }
  }

  // use a minimum value for batches
  if (opts.chunkSize < 1024 * 128) {
    opts.chunkSize = 1024 * 128;
  }

  auto clamped = std::clamp(opts.threadCount, uint32_t(1),
                            uint32_t(4 * NumberOfCores::getValue()));
  if (opts.threadCount != clamped) {
    LOG_TOPIC("53570", WARN, Logger::RESTORE)
        << "capping --threads value to " << clamped;
    opts.threadCount = clamped;
  }

  // validate shards and replication factor
  if (opts.defaultNumberOfShards == 0) {
    LOG_TOPIC("248ee", FATAL, arangodb::Logger::RESTORE)
        << "invalid value for `--default-number-of-shards`, expecting at least "
           "1";
    FATAL_ERROR_EXIT();
  }

  if (opts.defaultReplicationFactor == 0) {
    LOG_TOPIC("daf22", FATAL, arangodb::Logger::RESTORE)
        << "invalid value for `--default-replication-factor, expecting at "
           "least 1";
    FATAL_ERROR_EXIT();
  }

  for (auto& it : opts.numberOfShards) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() == 1 && basics::StringUtils::int64(parts[0]) > 0) {
      // valid
      continue;
    } else if (parts.size() == 2 && basics::StringUtils::int64(parts[1]) > 0) {
      // valid
      continue;
    }
    // invalid!
    LOG_TOPIC("1951e", FATAL, arangodb::Logger::RESTORE)
        << "got invalid value '" << it << "' for `--number-of-shards";
    FATAL_ERROR_EXIT();
  }

  for (auto& it : opts.replicationFactor) {
    auto parts = basics::StringUtils::split(it, '=');
    if (parts.size() == 1) {
      if (parts[0] == "satellite" || basics::StringUtils::int64(parts[0]) > 0) {
        // valid
        continue;
      }
    } else if (parts.size() == 2) {
      if (parts[1] == "satellite" || basics::StringUtils::int64(parts[1]) > 0) {
        // valid
        continue;
      }
    }
    // invalid!
    LOG_TOPIC("d038e", FATAL, arangodb::Logger::RESTORE)
        << "got invalid value '" << it << "' for `--replication-factor";
    FATAL_ERROR_EXIT();
  }
}

}  // namespace arangodb
