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

#include "Benchmark/BenchOptionsProvider.h"

#include "Benchmark/BenchmarkOperation.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

using namespace arangodb::options;

namespace arangodb {

void BenchOptionsProvider::declareOptions(std::shared_ptr<ProgramOptions> opts,
                                          BenchFeatureOptions& options) {
  opts->addSection("histogram", "Benchmark statistics configuration");
  opts->addOption("--histogram.interval-size",
                  "The bucket width, dynamically calculated by default: "
                  "`(first measured time * 20) / num-intervals`.",
                  new DoubleParameter(&options.histogramIntervalSize),
                  arangodb::options::makeDefaultFlags(options::Flags::Dynamic));
  opts->addOption("--histogram.num-intervals",
                  "The number of buckets (resolution).",
                  new UInt64Parameter(&options.histogramNumIntervals));
  opts->addOption(
      "--histogram.percentiles", "Which percentiles to calculate.",
      new VectorParameter<DoubleParameter>(&options.percentiles),
      arangodb::options::makeDefaultFlags(options::Flags::FlushOnFirst));
  opts->addOption(
          "--histogram.generate", "Display a histogram.",
          new BooleanParameter(&options.generateHistogram),
          arangodb::options::makeDefaultFlags(options::Flags::FlushOnFirst))
      .setIntroducedIn(31000);

  opts->addOption("--async", "Send asynchronous requests.",
                  new BooleanParameter(&options.async));

  opts->addOldOption("--concurrency", "threads");
  opts->addOption("--threads",
                  "The number of parallel threads and connections.",
                  new UInt64Parameter(&options.threadCount))
      .setIntroducedIn(31000);

  opts->addOption("--requests", "The total number of operations.",
                  new UInt64Parameter(&options.operations));

  opts->addObsoleteOption(
      "--batch-size", "number of operations in one batch (0 disables batching)",
      true);

  opts->addOption("--keep-alive", "Use HTTP keep-alive.",
                  new BooleanParameter(&options.keepAlive));

  opts->addOption(
      "--collection",
      "The collection name to use in tests (if they involve collections).",
      new StringParameter(&options.collection));

  opts->addOption(
      "--replication-factor",
      "The replication factor of created collections (cluster only).",
      new UInt64Parameter(&options.replicationFactor));

  opts->addOption("--number-of-shards",
                  "The number of shards of created collections (cluster only).",
                  new UInt64Parameter(&options.numberOfShards));

  opts->addOption("--wait-for-sync", "Use waitForSync for created collections.",
                  new BooleanParameter(&options.waitForSync));

  opts->addOption("--create-database",
                  "Whether to create the database specified via "
                  "the `--server.database` option.",
                  new BooleanParameter(&options.createDatabase));

  opts->addOption("--create-collection",
                  "Whether to create the collection specified via "
                  "the `--collection` option.",
                  new BooleanParameter(&options.createCollection))
      .setIntroducedIn(31000);

  opts->addOption("--duration",
                  "Test for a duration of this many seconds instead of a "
                  "fixed test count.",
                  new UInt64Parameter(&options.duration));

  std::unordered_set<std::string> cases;
  for (auto& [name, _] : arangobench::BenchmarkOperation::allBenchmarks()) {
    cases.emplace(name);
  }
  opts->addOption(
      "--test-case", "The test case to use.",
      new DiscreteValuesParameter<StringParameter>(&options.testCase, cases));

  opts->addOption(
      "--complexity",
      "The complexity parameter for the test (meaning depends on test case).",
      new UInt64Parameter(&options.complexity));

  opts->addOption("--delay",
                  "Use a startup delay (necessary only when run in series).",
                  new BooleanParameter(&options.delay));

  opts->addOption("--junit-report-file",
                  "The filename to write junit-style report to.",
                  new StringParameter(&options.junitReportFile));

  opts->addOption("--json-report-file",
                  "The filename to write a report in JSON format to.",
                  new StringParameter(&options.jsonReportFile));

  opts->addOption("--runs",
                  "Run test this many times (and calculate statistics based "
                  "on the median).",
                  new UInt64Parameter(&options.runs));

  opts->addOption("--progress", "Log intermediate progress.",
                  new BooleanParameter(&options.progress));

  opts->addOption("--custom-query",
                  "The query to be used in the \"custom-query\" test case.",
                  new StringParameter(&options.customQuery))
      .setIntroducedIn(30800);

  opts->addOption(
          "--custom-query-file",
          "A path to the file with the query to use in the \"custom-query\" "
          "test case. "
          "If `--custom-query` is specified as well, it has higher priority.",
          new StringParameter(&options.customQueryFile))
      .setIntroducedIn(30800);

  opts->addOption(
          "--custom-query-bindvars",
          "The bind parameters to be used in the \"custom-query\" test case.",
          new StringParameter(&options.customQueryBindVars))
      .setIntroducedIn(31000);

  opts->addOption("--quiet", "suppress status messages",
                  new BooleanParameter(&options.quiet));

  opts->addObsoleteOption(
      "--verbose",
      "Print out replies if the HTTP header indicates database errors.", false);
}

void BenchOptionsProvider::validateOptions(std::shared_ptr<ProgramOptions> opts,
                                           BenchFeatureOptions& options) {
  if (!options.generateHistogram) {
    if (opts->processingResult().touched("--histogram.interval-size")) {
      LOG_TOPIC("8b53b", WARN, arangodb::Logger::BENCH)
          << "For flag '--histogram.interval-size "
          << options.histogramIntervalSize
          << "': histogram is disabled by default. Enable it with flag "
             "'--histogram.generate = true'.";
    }
    if (opts->processingResult().touched("--histogram.num-intervals")) {
      LOG_TOPIC("02916", WARN, arangodb::Logger::BENCH)
          << "For flag '--histogram.num-intervals "
          << options.histogramNumIntervals
          << "': histogram is disabled by default. Enable it with flag "
             "'--histogram.generate = true'.";
    }
    if (opts->processingResult().touched("--histogram.percentiles")) {
      LOG_TOPIC("ad47b", WARN, arangodb::Logger::BENCH)
          << "For flag '--histogram.percentiles " << options.percentiles
          << "': histogram is disabled by default. Enable it with flag "
             "'--histogram.generate = true'.";
    }
  }
}

}  // namespace arangodb
