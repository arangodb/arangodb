////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "QueryRegistryFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/NumberOfCores.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

std::atomic<aql::QueryRegistry*> QueryRegistryFeature::QUERY_REGISTRY{nullptr};

QueryRegistryFeature::QueryRegistryFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "QueryRegistry"),
      _trackingEnabled(true),
      _trackSlowQueries(true),
      _trackQueryString(true),
      _trackBindVars(true),
      _trackDataSources(false),
      _failOnWarning(false),
      _queryCacheIncludeSystem(false),
      _smartJoins(true),
      _parallelizeTraversals(true),
      _queryMemoryLimit(0),
      _queryMaxRuntime(0.0),
      _maxQueryPlans(128),
      _queryCacheMaxResultsCount(0),
      _queryCacheMaxResultsSize(0),
      _queryCacheMaxEntrySize(0),
      _maxParallelism(4),
      _slowQueryThreshold(10.0),
      _slowStreamingQueryThreshold(10.0),
      _queryRegistryTTL(0.0),
      _queryCacheMode("off"),
      _queryTimes(
        server.getFeature<arangodb::MetricsFeature>().histogram(
          "arangodb_aql_query_time", log_scale_t(2., 0.0, 50.0, 20),
          "Execution time histogram for all AQL queries [s]")),
      _slowQueryTimes(
        server.getFeature<arangodb::MetricsFeature>().histogram(
          "arangodb_aql_slow_query_time", log_scale_t(2., 1.0, 2000.0, 10),
          "Execution time histogram for slow AQL queries [s]")),
      _totalQueryExecutionTime(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_aql_total_query_time_msec", 0, "Total execution time of all AQL queries [ms]")),
      _queriesCounter(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_aql_all_query", 0, "Total number of AQL queries")),
      _slowQueriesCounter(
        server.getFeature<arangodb::MetricsFeature>().counter(
          "arangodb_aql_slow_query", 0, "Number of slow AQL queries")) {
  setOptional(false);
  startsAfter<V8FeaturePhase>();

  auto properties = arangodb::aql::QueryCache::instance()->properties();
  _queryCacheMaxResultsCount = properties.maxResultsCount;
  _queryCacheMaxResultsSize = properties.maxResultsSize;
  _queryCacheMaxEntrySize = properties.maxEntrySize;
  _queryCacheIncludeSystem = properties.includeSystem;
}

void QueryRegistryFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("query", "Configure queries");

  options->addOldOption("database.query-cache-mode", "query.cache-mode");
  options->addOldOption("database.query-cache-max-results",
                        "query.cache-entries");
  options->addOldOption("database.disable-query-tracking", "query.tracking");

  options->addOption("--query.memory-limit",
                     "memory threshold for AQL queries (in bytes, 0 = no limit)",
                     new UInt64Parameter(&_queryMemoryLimit));
  
  options->addOption("--query.max-runtime",
                     "runtime threshold for AQL queries (in seconds, 0 = no limit)",
                     new DoubleParameter(&_queryMaxRuntime))
                     .setIntroducedIn(30607).setIntroducedIn(30703);

  options->addOption("--query.tracking", "whether to track queries",
                     new BooleanParameter(&_trackingEnabled));

  options->addOption("--query.tracking-slow-queries", "whether to track slow queries",
                     new BooleanParameter(&_trackSlowQueries))
                     .setIntroducedIn(30704);

  options->addOption("--query.tracking-with-querystring", "whether to track the query string",
                     new BooleanParameter(&_trackQueryString))
                     .setIntroducedIn(30704);

  options->addOption("--query.tracking-with-bindvars",
                     "whether to track bind vars with AQL queries",
                     new BooleanParameter(&_trackBindVars));
  
  options->addOption("--query.tracking-with-datasources", "whether to track data sources with AQL queries",
                     new BooleanParameter(&_trackDataSources))
                     .setIntroducedIn(30704);

  options->addOption("--query.fail-on-warning",
                     "whether AQL queries should fail with errors even for "
                     "recoverable warnings",
                     new BooleanParameter(&_failOnWarning));

  options->addOption("--query.slow-threshold",
                     "threshold for slow AQL queries (in seconds)",
                     new DoubleParameter(&_slowQueryThreshold));

  options->addOption("--query.slow-streaming-threshold",
                     "threshold for slow streaming AQL queries (in seconds)",
                     new DoubleParameter(&_slowStreamingQueryThreshold));

  options->addOption("--query.cache-mode",
                     "mode for the AQL query result cache (on, off, demand)",
                     new StringParameter(&_queryCacheMode));

  options->addOption(
      "--query.cache-entries",
      "maximum number of results in query result cache per database",
      new UInt64Parameter(&_queryCacheMaxResultsCount));

  options->addOption(
      "--query.cache-entries-max-size",
      "maximum cumulated size of results in query result cache per database",
      new UInt64Parameter(&_queryCacheMaxResultsSize));

  options->addOption(
      "--query.cache-entry-max-size",
      "maximum size of an invidiual result entry in query result cache",
      new UInt64Parameter(&_queryCacheMaxEntrySize));

  options->addOption("--query.cache-include-system-collections",
                     "whether or not to include system collection queries in "
                     "the query result cache",
                     new BooleanParameter(&_queryCacheIncludeSystem));

  options->addOption("--query.optimizer-max-plans",
                     "maximum number of query plans to create for a query",
                     new UInt64Parameter(&_maxQueryPlans));

  options->addOption("--query.registry-ttl",
                     "default time-to-live of cursors and query snippets (in "
                     "seconds); if <= 0, value will default to 30 for "
                     "single-server instances or 600 for cluster instances",
                     new DoubleParameter(&_queryRegistryTTL),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
  
  options->addOption("--query.smart-joins",
                     "enable SmartJoins query optimization",
                     new BooleanParameter(&_smartJoins),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden, arangodb::options::Flags::Enterprise))
                     .setIntroducedIn(30405).setIntroducedIn(30500);
  
  options->addOption("--query.parallelize-traversals",
                     "enable traversal parallelization",
                     new BooleanParameter(&_parallelizeTraversals),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden, arangodb::options::Flags::Enterprise))
                     .setIntroducedIn(30701);

  options
      ->addOption(
          "--query.max-parallelism",
          "maximum number of threads to use for a single query; "
          "actual query execution may use less depending on various factors",
          new UInt64Parameter(&_maxParallelism),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                              arangodb::options::Flags::Enterprise))
      .setIntroducedIn(30701);
}

void QueryRegistryFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_queryMaxRuntime < 0.0) {
    LOG_TOPIC("46572", FATAL, Logger::AQL)
        << "invalid value for `--query.max-runtime`. expecting 0 or a positive value";
    FATAL_ERROR_EXIT();
  }

  if (_maxQueryPlans == 0) {
    LOG_TOPIC("4006f", FATAL, Logger::AQL)
        << "invalid value for `--query.optimizer-max-plans`. expecting at "
           "least 1";
    FATAL_ERROR_EXIT();
  }

  // cap the value somehow. creating this many plans really does not make sense
  _maxQueryPlans = std::min(_maxQueryPlans, decltype(_maxQueryPlans)(1024));
  
  _maxParallelism = std::clamp(_maxParallelism, static_cast<uint64_t>(1),
                               static_cast<uint64_t>(NumberOfCores::getValue()));
}

void QueryRegistryFeature::prepare() {
  if (ServerState::instance()->isCoordinator()) {
    // turn the query cache off on the coordinator, as it is not implemented
    // for the cluster
    _queryCacheMode = "off";
  }

  // configure the query cache
  arangodb::aql::QueryCacheProperties properties{arangodb::aql::QueryCache::modeString(_queryCacheMode),
                                                 _queryCacheMaxResultsCount,
                                                 _queryCacheMaxResultsSize,
                                                 _queryCacheMaxEntrySize,
                                                 _queryCacheIncludeSystem,
                                                 _trackBindVars};
  arangodb::aql::QueryCache::instance()->properties(properties);

  if (_queryRegistryTTL <= 0) {
    // set to default value based on instance type
    _queryRegistryTTL = ServerState::instance()->isSingleServer() ? 30 : 600;
  }

  // create the query registery
  _queryRegistry.reset(new aql::QueryRegistry(_queryRegistryTTL));
  QUERY_REGISTRY.store(_queryRegistry.get(), std::memory_order_release);
}

void QueryRegistryFeature::start() {}

void QueryRegistryFeature::beginShutdown() {
  TRI_ASSERT(_queryRegistry != nullptr);
  _queryRegistry->disallowInserts();
}

void QueryRegistryFeature::stop() {
  TRI_ASSERT(_queryRegistry != nullptr);
  _queryRegistry->disallowInserts();
  _queryRegistry->destroyAll();
}

void QueryRegistryFeature::unprepare() {
  // clear the query registery
  QUERY_REGISTRY.store(nullptr, std::memory_order_release);
}

void QueryRegistryFeature::trackQuery(double time) { 
  ++_queriesCounter; 
  _queryTimes.count(time);
  _totalQueryExecutionTime += static_cast<uint64_t>(1000.0 * time);
}

void QueryRegistryFeature::trackSlowQuery(double time) { 
  // query is already counted here as normal query, so don't count it
  // again in _totalQueryExecutionTime
  ++_slowQueriesCounter; 
  _slowQueryTimes.count(time);
}

}  // namespace arangodb
