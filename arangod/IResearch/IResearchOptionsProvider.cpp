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

#include "IResearch/IResearchOptionsProvider.h"

#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Basics/debugging.h"
#include "IResearch/IResearchCommon.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"

namespace arangodb::iresearch {

namespace {

std::string const COMMIT_THREADS_PARAM("--arangosearch.commit-threads");
std::string const COMMIT_THREADS_IDLE_PARAM(
    "--arangosearch.commit-threads-idle");
std::string const CONSOLIDATION_THREADS_PARAM(
    "--arangosearch.consolidation-threads");
std::string const CONSOLIDATION_THREADS_IDLE_PARAM(
    "--arangosearch.consolidation-threads-idle");
std::string const FAIL_ON_OUT_OF_SYNC(
    "--arangosearch.fail-queries-on-out-of-sync");
std::string const SEARCH_THREADS_LIMIT(
    "--arangosearch.execution-threads-limit");
std::string const SEARCH_DEFAULT_PARALLELISM(
    "--arangosearch.default-parallelism");

uint32_t computeThreadsCount(uint32_t threads, uint32_t threadsLimit,
                             uint32_t div) noexcept {
  TRI_ASSERT(div);
  // arbitrary limit on the upper bound of threads in pool
  constexpr uint32_t MAX_THREADS = 8;
  constexpr uint32_t MIN_THREADS = 1;  // at least one thread is required

  return std::max(
      MIN_THREADS,
      std::min(threadsLimit ? threadsLimit : MAX_THREADS,
               threads ? threads : uint32_t(NumberOfCores::getValue()) / div));
}

}  // namespace

const std::string IResearchOptionsProvider::SKIP_RECOVERY{
    "--arangosearch.skip-recovery"};
const std::string IResearchOptionsProvider::CACHE_LIMIT{
    "--arangosearch.columns-cache-limit"};
const std::string IResearchOptionsProvider::CACHE_ONLY_LEADER{
    "--arangosearch.columns-cache-only-leader"};

void IResearchOptionsProvider::declareOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options, IResearchOptions& opts) {
  options->addSection("arangosearch", "ArangoSearch feature");

  options
      ->addOption(
          CONSOLIDATION_THREADS_PARAM,
          "The upper limit to the allowed number of consolidation threads "
          "(0 = auto-detect).",
          new options::UInt32Parameter(&opts.consolidationThreads))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..arangosearch.consolidation-threads ]`. Set it to `0` to automatically
choose a sensible number based on the number of cores in the system.)");

  options
      ->addOption(
          CONSOLIDATION_THREADS_IDLE_PARAM,
          "The upper limit to the allowed number of idle threads to use "
          "for consolidation tasks (0 = auto-detect).",
          new options::UInt32Parameter(&opts.deprecatedOptions))
      .setDeprecatedIn(3'11'06)
      .setDeprecatedIn(3'12'00);

  options
      ->addOption(COMMIT_THREADS_PARAM,
                  "The upper limit to the allowed number of commit threads "
                  "(0 = auto-detect).",
                  new options::UInt32Parameter(&opts.commitThreads))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..4 * NumberOfCores ]`. Set it to `0` to automatically choose a sensible
number based on the number of cores in the system.)");

  options
      ->addOption(
          COMMIT_THREADS_IDLE_PARAM,
          "The upper limit to the allowed number of idle threads to use "
          "for commit tasks (0 = auto-detect)",
          new options::UInt32Parameter(&opts.deprecatedOptions))
      .setLongDescription(R"(The option value must fall in the range
`[ 1..arangosearch.commit-threads ]`. Set it to `0` to automatically choose a
sensible number based on the number of cores in the system.)")
      .setDeprecatedIn(3'11'06)
      .setDeprecatedIn(3'12'00);

  options
      ->addOption(
          SKIP_RECOVERY,  // TODO: Move parts of the descriptions to
                          // longDescription?
          "Skip the data recovery for the specified View link or inverted "
          "index on startup. The value for this option needs to have the "
          "format '<collection-name>/<index-id>' or "
          "'<collection-name>/<index-name>'. You can use the option multiple "
          "times, for each View link and inverted index to skip the recovery "
          "for. The pseudo-value 'all' disables the recovery for all View "
          "links and inverted indexes. The links/indexes skipped during the "
          "recovery are marked as out-of-sync when the recovery completes. You "
          "need to recreate them manually afterwards.\n"
          "WARNING: Using this option causes data of affected links/indexes to "
          "become incomplete or more incomplete until they have been manually "
          "recreated.",
          new options::VectorParameter<options::StringParameter>(
              &opts.skipRecoveryItems))
      .setIntroducedIn(30904);

  options
      ->addOption(FAIL_ON_OUT_OF_SYNC,
                  "Whether retrieval queries on out-of-sync "
                  "View links and inverted indexes should fail.",
                  new options::BooleanParameter(&opts.failQueriesOnOutOfSync))
      .setIntroducedIn(30904)
      .setLongDescription(R"(If set to `true`, any data retrieval queries on
out-of-sync links/indexes fail with the error 'collection/view is out of sync'
(error code 1481).

If set to `false`, queries on out-of-sync links/indexes are answered normally,
but the returned data may be incomplete.)");

  options
      ->addOption(
          SEARCH_THREADS_LIMIT,
          "The maximum number of threads that can be used to process "
          "ArangoSearch indexes during a SEARCH operation of a query.",
          new options::UInt32Parameter(&opts.searchExecutionThreadsLimit),
          options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                    options::Flags::OnDBServer,
                                    options::Flags::OnSingle))
      .setIntroducedIn(3'11'06)
      .setIntroducedIn(3'12'00);
  options
      ->addOption(SEARCH_DEFAULT_PARALLELISM,
                  "Default parallelism for ArangoSearch queries",
                  new options::UInt32Parameter(&opts.defaultParallelism),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnDBServer,
                                            options::Flags::OnSingle))
      .setIntroducedIn(3'11'06)
      .setIntroducedIn(3'12'00);

#ifdef USE_ENTERPRISE
  options
      ->addOption(IResearchOptionsProvider::CACHE_LIMIT,
                  "The limit (in bytes) for ArangoSearch columns cache "
                  "(0 = no caching).",
                  new options::UInt64Parameter(&opts.columnsCacheLimit),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnSingle,
                                            options::Flags::OnDBServer,
                                            options::Flags::Enterprise))
      .setIntroducedIn(3'09'05);
  options
      ->addOption(IResearchOptionsProvider::CACHE_ONLY_LEADER,
                  "Cache ArangoSearch columns only for leader shards.",
                  new options::BooleanParameter(&opts.columnsCacheOnlyLeader),
                  options::makeDefaultFlags(options::Flags::DefaultNoComponents,
                                            options::Flags::OnDBServer,
                                            options::Flags::Enterprise))
      .setIntroducedIn(3'10'06);
#endif
}

void IResearchOptionsProvider::validateOptionsImpl(
    std::shared_ptr<options::ProgramOptions> options, IResearchOptions& opts) {
  // validate all entries in skipRecoveryItems for formal correctness
  auto checkFormat = [](auto const& item) {
    auto r = item.find('/');
    if (r == std::string_view::npos) {
      return false;
    }
    r = item.find('/', r);
    if (r == std::string_view::npos) {
      return true;
    }
    return false;
  };
  for (auto const& item : opts.skipRecoveryItems) {
    if (item != "all" && checkFormat(item)) {
      LOG_TOPIC("b9f28", FATAL, arangodb::iresearch::TOPIC)
          << "invalid format for '" << SKIP_RECOVERY
          << "' parameter. expecting '"
          << "<collection-name>/<index-id>' or "
             "'<collection-name>/<index-name>' or "
          << "'all', got: '" << item << "'";
      FATAL_ERROR_EXIT();
    }
  }

  uint32_t threadsLimit = static_cast<uint32_t>(4 * NumberOfCores::getValue());
  opts.commitThreads = computeThreadsCount(opts.commitThreads, threadsLimit, 6);
  opts.consolidationThreads =
      computeThreadsCount(opts.consolidationThreads, threadsLimit, 6);

  auto const& args = options->processingResult();
  if (!args.touched(SEARCH_THREADS_LIMIT)) {
    opts.searchExecutionThreadsLimit =
        static_cast<uint32_t>(2 * NumberOfCores::getValue());
  }
}

}  // namespace arangodb::iresearch
