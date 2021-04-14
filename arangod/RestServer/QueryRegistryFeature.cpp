////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include <algorithm>

#include "QueryRegistryFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/Query.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/application-exit.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/V8FeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "RestServer/MetricsFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace {

uint64_t defaultMemoryLimit(uint64_t available, double reserveFraction, double percentage) {
  if (available == 0) {
    // we don't know how much memory is available, so we cannot do any sensible calculation
    return 0;
  }

  // this function will produce the following results for a reserveFraction of 0.2 and a 
  // percentage of 0.75 for some common available memory values:
  //
  //    Available memory:            0      (0MiB)  Limit:            0   unlimited, %mem:  n/a
  //    Available memory:    134217728    (128MiB)  Limit:     33554432     (32MiB), %mem: 25.0
  //    Available memory:    268435456    (256MiB)  Limit:     67108864     (64MiB), %mem: 25.0
  //    Available memory:    536870912    (512MiB)  Limit:    201326592    (192MiB), %mem: 37.5
  //    Available memory:    805306368    (768MiB)  Limit:    402653184    (384MiB), %mem: 50.0
  //    Available memory:   1073741824   (1024MiB)  Limit:    603979776    (576MiB), %mem: 56.2
  //    Available memory:   2147483648   (2048MiB)  Limit:   1288490189   (1228MiB), %mem: 60.0
  //    Available memory:   4294967296   (4096MiB)  Limit:   2576980377   (2457MiB), %mem: 60.0
  //    Available memory:   8589934592   (8192MiB)  Limit:   5153960755   (4915MiB), %mem: 60.0
  //    Available memory:  17179869184  (16384MiB)  Limit:  10307921511   (9830MiB), %mem: 60.0
  //    Available memory:  25769803776  (24576MiB)  Limit:  15461882265  (14745MiB), %mem: 60.0
  //    Available memory:  34359738368  (32768MiB)  Limit:  20615843021  (19660MiB), %mem: 60.0
  //    Available memory:  42949672960  (40960MiB)  Limit:  25769803776  (24576MiB), %mem: 60.0
  //    Available memory:  68719476736  (65536MiB)  Limit:  41231686041  (39321MiB), %mem: 60.0
  //    Available memory: 103079215104  (98304MiB)  Limit:  61847529063  (58982MiB), %mem: 60.0
  //    Available memory: 137438953472 (131072MiB)  Limit:  82463372083  (78643MiB), %mem: 60.0
  //    Available memory: 274877906944 (262144MiB)  Limit: 164926744167 (157286MiB), %mem: 60.0
  //    Available memory: 549755813888 (524288MiB)  Limit: 329853488333 (314572MiB), %mem: 60.0
  
  // for a reserveFraction of 0.05 and a percentage of 0.95 it will produce:
  //
  //    Available memory:            0      (0MiB)  Limit:            0   unlimited, %mem:  n/a
  //    Available memory:    134217728    (128MiB)  Limit:     33554432     (32MiB), %mem: 25.0
  //    Available memory:    268435456    (256MiB)  Limit:     67108864     (64MiB), %mem: 25.0
  //    Available memory:    536870912    (512MiB)  Limit:    255013683    (243MiB), %mem: 47.5
  //    Available memory:    805306368    (768MiB)  Limit:    510027366    (486MiB), %mem: 63.3
  //    Available memory:   1073741824   (1024MiB)  Limit:    765041049    (729MiB), %mem: 71.2
  //    Available memory:   2147483648   (2048MiB)  Limit:   1785095782   (1702MiB), %mem: 83.1
  //    Available memory:   4294967296   (4096MiB)  Limit:   3825205248   (3648MiB), %mem: 89.0
  //    Available memory:   8589934592   (8192MiB)  Limit:   7752415969   (7393MiB), %mem: 90.2
  //    Available memory:  17179869184  (16384MiB)  Limit:  15504831938  (14786MiB), %mem: 90.2
  //    Available memory:  25769803776  (24576MiB)  Limit:  23257247908  (22179MiB), %mem: 90.2
  //    Available memory:  34359738368  (32768MiB)  Limit:  31009663877  (29573MiB), %mem: 90.2
  //    Available memory:  42949672960  (40960MiB)  Limit:  38762079846  (36966MiB), %mem: 90.2
  //    Available memory:  68719476736  (65536MiB)  Limit:  62019327755  (59146MiB), %mem: 90.2
  //    Available memory: 103079215104  (98304MiB)  Limit:  93028991631  (88719MiB), %mem: 90.2
  //    Available memory: 137438953472 (131072MiB)  Limit: 124038655509 (118292MiB), %mem: 90.2
  //    Available memory: 274877906944 (262144MiB)  Limit: 248077311017 (236584MiB), %mem: 90.2
  //    Available memory: 549755813888 (524288MiB)  Limit: 496154622034 (473169MiB), %mem: 90.2
  
    // reserveFraction% of RAM will be considered as a reserve
  uint64_t reserve = static_cast<uint64_t>(available * reserveFraction);

  // minimum reserve memory is 256MB
  reserve = std::max<uint64_t>(reserve, static_cast<uint64_t>(256) << 20);

  double f = double(1.0) - (double(reserve) / double(available));
  double dyn = (double(available) * f * percentage);
  if (dyn < 0.0) {
    dyn = 0.0;
  }

  return std::max(static_cast<uint64_t>(dyn), static_cast<uint64_t>(0.25 * available));
}

} // namespace

namespace arangodb {

std::atomic<aql::QueryRegistry*> QueryRegistryFeature::QUERY_REGISTRY{nullptr};

struct QueryTimeScale {
  static log_scale_t<double> scale() { return {2., 0.0, 50.0, 20}; }
};
struct SlowQueryTimeScale {
  static log_scale_t<double> scale() { return {2., 1.0, 2000.0, 10}; }
};

DECLARE_COUNTER(arangodb_aql_all_query_total,
                "Total number of AQL queries finished");
DECLARE_HISTOGRAM(arangodb_aql_query_time, QueryTimeScale,
                  "Execution time histogram for all AQL queries [s]");
DECLARE_LEGACY_COUNTER(arangodb_aql_slow_query_total,
                "Total number of slow AQL queries finished");
DECLARE_HISTOGRAM(arangodb_aql_slow_query_time, SlowQueryTimeScale,
                  "Execution time histogram for slow AQL queries [s]");
DECLARE_COUNTER(arangodb_aql_total_query_time_msec_total,
                "Total execution time of all AQL queries [ms]");
DECLARE_GAUGE(arangodb_aql_current_query, uint64_t,
              "Current number of AQL queries executing");
DECLARE_GAUGE(
    arangodb_aql_global_memory_usage, uint64_t,
    "Total memory usage of all AQL queries executing [bytes], granularity: " +
        std::to_string(ResourceMonitor::chunkSize) + " bytes steps");
DECLARE_GAUGE(arangodb_aql_global_memory_limit, uint64_t,
              "Total memory limit for all AQL queries combined [bytes]");
DECLARE_COUNTER(arangodb_aql_global_query_memory_limit_reached,
                "Number of global AQL query memory limit violations");
DECLARE_COUNTER(arangodb_aql_local_query_memory_limit_reached,
                "Number of local AQL query memory limit violations");

QueryRegistryFeature::QueryRegistryFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "QueryRegistry"),
      _trackingEnabled(true),
      _trackSlowQueries(true),
      _trackQueryString(true),
      _trackBindVars(true),
      _trackDataSources(false),
      _failOnWarning(aql::QueryOptions::defaultFailOnWarning),
      _queryCacheIncludeSystem(false),
      _queryMemoryLimitOverride(true),
#ifdef USE_ENTERPRISE
      _smartJoins(true),
      _parallelizeTraversals(true),
#endif
      _allowCollectionsInExpressions(false),
      _queryGlobalMemoryLimit(defaultMemoryLimit(PhysicalMemory::getValue(), 0.1, 0.90)),
      _queryMemoryLimit(defaultMemoryLimit(PhysicalMemory::getValue(), 0.2, 0.75)),
      _queryMaxRuntime(aql::QueryOptions::defaultMaxRuntime),
      _maxQueryPlans(aql::QueryOptions::defaultMaxNumberOfPlans),
      _queryCacheMaxResultsCount(0),
      _queryCacheMaxResultsSize(0),
      _queryCacheMaxEntrySize(0),
      _maxParallelism(4),
      _slowQueryThreshold(10.0),
      _slowStreamingQueryThreshold(10.0),
      _queryRegistryTTL(0.0),
      _queryCacheMode("off"),
      _queryTimes(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_query_time{})),
      _slowQueryTimes(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_slow_query_time{})),
      _totalQueryExecutionTime(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_total_query_time_msec_total{})),
      _queriesCounter(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_all_query_total{})),
      _slowQueriesCounter(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_slow_query_total{})),
      _runningQueries(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_current_query{})),
      _globalQueryMemoryUsage(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_global_memory_usage{})),
      _globalQueryMemoryLimit(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_global_memory_limit{})),
      _globalQueryMemoryLimitReached(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_global_query_memory_limit_reached{})),
      _localQueryMemoryLimitReached(
        server.getFeature<arangodb::MetricsFeature>().add(arangodb_aql_local_query_memory_limit_reached{})) {
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
  
  options->addOption("--query.global-memory-limit",
                     "memory threshold for all AQL queries combined (in bytes, 0 = no limit)",
                     new UInt64Parameter(&_queryGlobalMemoryLimit, PhysicalMemory::getValue()),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic))
                     .setIntroducedIn(30800);

  options->addOption("--query.memory-limit",
                     "memory threshold per AQL query (in bytes, 0 = no limit)",
                     new UInt64Parameter(&_queryMemoryLimit, PhysicalMemory::getValue()),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Dynamic));
  
  options->addOption("--query.memory-limit-override",
                     "allow increasing per-query memory limits for individual queries",
                     new BooleanParameter(&_queryMemoryLimitOverride))
                     .setIntroducedIn(30800);
  
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
  
#ifdef USE_ENTERPRISE
  options->addOption("--query.smart-joins",
                     "enable SmartJoins query optimization",
                     new BooleanParameter(&_smartJoins),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden, arangodb::options::Flags::Enterprise))
                     .setIntroducedIn(30405);
  
  options->addOption("--query.parallelize-traversals",
                     "enable traversal parallelization",
                     new BooleanParameter(&_parallelizeTraversals),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden, arangodb::options::Flags::Enterprise))
                     .setIntroducedIn(30701);

  // this is an Enterprise-only option
  // in Community Edition, _maxParallelism will stay at its default value
  // (currently 4), but will not be used.
  options
      ->addOption(
          "--query.max-parallelism",
          "maximum number of threads to use for a single query; "
          "actual query execution may use less depending on various factors",
          new UInt64Parameter(&_maxParallelism),
          arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden,
                                              arangodb::options::Flags::Enterprise))
      .setIntroducedIn(30701);
#endif
  
  options->addOption("--query.allow-collections-in-expressions",
                     "allow full collections to be used in AQL expressions",
                     new BooleanParameter(&_allowCollectionsInExpressions),
                     arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden))
                     .setIntroducedIn(30800).setDeprecatedIn(30900);
}

void QueryRegistryFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_queryGlobalMemoryLimit > 0 &&
      _queryMemoryLimit > _queryGlobalMemoryLimit) {
    LOG_TOPIC("2af5f", FATAL, Logger::AQL)
        << "invalid value for `--query.global-memory-limit`. expecting 0 or a value >= `--query.memory-limit`";
    FATAL_ERROR_EXIT();
  }

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
                               
  if (_queryRegistryTTL <= 0) {
    TRI_ASSERT(ServerState::instance()->getRole() != ServerState::ROLE_UNDEFINED);
    // set to default value based on instance type
    _queryRegistryTTL = ServerState::instance()->isSingleServer() ? 30 : 600;
  }

  TRI_ASSERT(_queryGlobalMemoryLimit == 0 || _queryMemoryLimit <= _queryGlobalMemoryLimit);
  
  aql::QueryOptions::defaultMemoryLimit = _queryMemoryLimit;
  aql::QueryOptions::defaultMaxNumberOfPlans = _maxQueryPlans;
  aql::QueryOptions::defaultMaxRuntime = _queryMaxRuntime;
  aql::QueryOptions::defaultTtl = _queryRegistryTTL;
  aql::QueryOptions::defaultFailOnWarning = _failOnWarning;
  aql::QueryOptions::allowMemoryLimitOverride = _queryMemoryLimitOverride;
}

void QueryRegistryFeature::prepare() {
  // set the global memory limit
  GlobalResourceMonitor::instance().memoryLimit(_queryGlobalMemoryLimit);
  // prepare gauge value
  _globalQueryMemoryLimit = _queryGlobalMemoryLimit;
  
#ifndef ARANGODB_USE_GOOGLE_TESTS
  // we are now intentionally not printing this message during testing,
  // because otherwise it would be printed a *lot* of times
  // note that options() can be a nullptr during unit testing
  if (server().options() != nullptr &&
      !server().options()->processingResult().touched("--query.memory-limit")) {
    LOG_TOPIC("f6e0e", INFO, Logger::AQL)
        << "memory limit per AQL query automatically set to " << _queryMemoryLimit << " bytes. "
        << "to modify this value, please adjust the startup option `--query.memory-limit`";
  }
#endif
  
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
  // create the query registry
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
  // clear the query registry
  QUERY_REGISTRY.store(nullptr, std::memory_order_release);
}

void QueryRegistryFeature::updateMetrics() {
  GlobalResourceMonitor const& global = GlobalResourceMonitor::instance();
  _globalQueryMemoryUsage = global.current();
  _globalQueryMemoryLimit = global.memoryLimit();

  auto stats = global.stats();
  _globalQueryMemoryLimitReached = stats.globalLimitReached;
  _localQueryMemoryLimitReached = stats.localLimitReached;
}

void QueryRegistryFeature::trackQueryStart() noexcept {
  ++_runningQueries;
}

void QueryRegistryFeature::trackQueryEnd(double time) { 
  ++_queriesCounter; 
  _queryTimes.count(time);
  _totalQueryExecutionTime += static_cast<uint64_t>(1000.0 * time);
  --_runningQueries;
}

void QueryRegistryFeature::trackSlowQuery(double time) { 
  // query is already counted here as normal query, so don't count it
  // again in _queryTimes or _totalQueryExecutionTime
  ++_slowQueriesCounter; 
  _slowQueryTimes.count(time);
}

}  // namespace arangodb
