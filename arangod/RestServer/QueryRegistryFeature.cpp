////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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

#include "QueryRegistryFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryOptions.h"
#include "Aql/QueryRegistry.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Cluster/ServerState.h"
#include "FeaturePhases/ClusterFeaturePhase.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/Parameters.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/QueryRegistryOptionsProvider.h"
#include "Metrics/CounterBuilder.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/HistogramBuilder.h"
#include "Metrics/LogScale.h"
#include "Metrics/IRegistry.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

std::atomic<aql::QueryRegistry*> QueryRegistryFeature::QUERY_REGISTRY{nullptr};

struct QueryTimeScale {
  static metrics::LogScale<double> scale() { return {2., 0.0, 50.0, 20}; }
};
struct SlowQueryTimeScale {
  static metrics::LogScale<double> scale() { return {2., 1.0, 2000.0, 10}; }
};

DECLARE_COUNTER(arangodb_aql_all_query_total,
                "Total number of AQL queries finished");
DECLARE_HISTOGRAM(arangodb_aql_query_time, QueryTimeScale,
                  "Execution time histogram for all AQL queries [s]");
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
DECLARE_COUNTER(arangodb_aql_global_query_memory_limit_reached_total,
                "Number of global AQL query memory limit violations");
DECLARE_COUNTER(arangodb_aql_local_query_memory_limit_reached_total,
                "Number of local AQL query memory limit violations");
DECLARE_GAUGE(arangodb_aql_cursors_active, uint64_t,
              "Total amount of active AQL query results cursors");
DECLARE_GAUGE(arangodb_aql_cursors_memory_usage, uint64_t,
              "Total memory usage of active query result cursors");
DECLARE_COUNTER(arangodb_aql_query_plan_cache_hits_total,
                "Total number of lookup hits in the AQL query plan cache");
DECLARE_COUNTER(arangodb_aql_query_plan_cache_misses_total,
                "Total number of lookup misses in the AQL query plan cache");
DECLARE_GAUGE(
    arangodb_aql_query_plan_cache_memory_usage, uint64_t,
    "Total memory usage of the AQL query plan cache across all databases");

QueryRegistryFeature::QueryRegistryFeature(ApplicationServer& server,
                                           metrics::IRegistry& metricsRegistry)
    : QueryRegistryFeature(server, metricsRegistry,
                           QueryRegistryFeatureOptions{}) {}

QueryRegistryFeature::QueryRegistryFeature(ApplicationServer& server,
                                           metrics::IRegistry& metricsRegistry,
                                           QueryRegistryFeatureOptions options)
    : ApplicationFeature{server, *this},
      _options(std::move(options)),
      _queryTimes(metricsRegistry.add(arangodb_aql_query_time{})),
      _slowQueryTimes(metricsRegistry.add(arangodb_aql_slow_query_time{})),
      _totalQueryExecutionTime(
          metricsRegistry.add(arangodb_aql_total_query_time_msec_total{})),
      _queriesCounter(metricsRegistry.add(arangodb_aql_all_query_total{})),
      _runningQueries(metricsRegistry.add(arangodb_aql_current_query{})),
      _globalQueryMemoryUsage(
          metricsRegistry.add(arangodb_aql_global_memory_usage{})),
      _globalQueryMemoryLimit(
          metricsRegistry.add(arangodb_aql_global_memory_limit{})),
      _globalQueryMemoryLimitReached(metricsRegistry.add(
          arangodb_aql_global_query_memory_limit_reached_total{})),
      _localQueryMemoryLimitReached(metricsRegistry.add(
          arangodb_aql_local_query_memory_limit_reached_total{})),
      _activeCursors(metricsRegistry.add(arangodb_aql_cursors_active{})),
      _cursorsMemoryUsage(
          metricsRegistry.add(arangodb_aql_cursors_memory_usage{})),
      _queryPlanCacheHitsMetric(
          metricsRegistry.add(arangodb_aql_query_plan_cache_hits_total{})),
      _queryPlanCacheMissesMetric(
          metricsRegistry.add(arangodb_aql_query_plan_cache_misses_total{})),
      _queryPlanCacheMemoryUsage(
          metricsRegistry.add(arangodb_aql_query_plan_cache_memory_usage{})) {
  setOptional(false);
  startsAfter<application_features::ClusterFeaturePhase>();

  auto properties = arangodb::aql::QueryCache::instance()->properties();
  _options.queryCacheMaxResultsCount = properties.maxResultsCount;
  _options.queryCacheMaxResultsSize = properties.maxResultsSize;
  _options.queryCacheMaxEntrySize = properties.maxEntrySize;
  _options.queryCacheIncludeSystem = properties.includeSystem;
}

QueryRegistryFeature::~QueryRegistryFeature() = default;

void QueryRegistryFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  QueryRegistryOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void QueryRegistryFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  QueryRegistryOptionsProvider provider;
  provider.validateOptions(options, _options);

  aql::QueryOptions::defaultMemoryLimit = _options.queryMemoryLimit;
  aql::QueryOptions::defaultMaxNumberOfPlans = _options.maxQueryPlans;
  aql::QueryOptions::defaultMaxNodesPerCallstack =
      _options.maxNodesPerCallstack;
  aql::QueryOptions::defaultMaxDNFConditionMembers =
      _options.maxDNFConditionMembers;
  aql::QueryOptions::defaultMaxRuntime = _options.queryMaxRuntime;
  aql::QueryOptions::defaultTtl = _options.queryRegistryTTL;
  aql::QueryOptions::defaultFailOnWarning = _options.failOnWarning;
  aql::QueryOptions::allowMemoryLimitOverride =
      _options.queryMemoryLimitOverride;
}

void QueryRegistryFeature::prepare() {
  // set the global memory limit
  GlobalResourceMonitor::instance().memoryLimit(
      _options.queryGlobalMemoryLimit);
  // prepare gauge value
  _globalQueryMemoryLimit = _options.queryGlobalMemoryLimit;

#ifndef ARANGODB_USE_GOOGLE_TESTS
  // we are now intentionally not printing this message during testing,
  // because otherwise it would be printed a *lot* of times
  // note that options() can be a nullptr during unit testing
  if (server().options() != nullptr &&
      !server().options()->processingResult().touched("--query.memory-limit")) {
    LOG_TOPIC("f6e0e", INFO, Logger::AQL)
        << "memory limit per AQL query automatically set to "
        << _options.queryMemoryLimit << " bytes. "
        << "to modify this value, please adjust the startup option "
           "`--query.memory-limit`";
  }
#endif

  if (ServerState::instance()->isCoordinator()) {
    // turn the query cache off on the coordinator, as it is not implemented
    // for the cluster
    _options.queryCacheMode = "off";
  }

  // configure the query cache
  arangodb::aql::QueryCacheProperties properties{
      arangodb::aql::QueryCache::modeString(_options.queryCacheMode),
      _options.queryCacheMaxResultsCount,
      _options.queryCacheMaxResultsSize,
      _options.queryCacheMaxEntrySize,
      _options.queryCacheIncludeSystem,
      _options.trackBindVars};
  arangodb::aql::QueryCache::instance()->properties(properties);
  // create the query registry
  _queryRegistry =
      std::make_unique<aql::QueryRegistry>(_options.queryRegistryTTL);
  QUERY_REGISTRY.store(_queryRegistry.get(), std::memory_order_release);

  _asyncPrefetchSlotsManager.configure(_options.maxAsyncPrefetchSlotsTotal,
                                       _options.maxAsyncPrefetchSlotsPerQuery);
}

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

void QueryRegistryFeature::trackQueryStart() noexcept { ++_runningQueries; }

void QueryRegistryFeature::trackQueryEnd(double time) {
  ++_queriesCounter;
  _queryTimes.count(time);
  _totalQueryExecutionTime += static_cast<uint64_t>(1000.0 * time);
  --_runningQueries;
}

void QueryRegistryFeature::trackSlowQuery(double time) {
  // query is already counted here as normal query, so don't count it
  // again in _queryTimes or _totalQueryExecutionTime
  _slowQueryTimes.count(time);
}

aql::AsyncPrefetchSlotsManager&
QueryRegistryFeature::asyncPrefetchSlotsManager() noexcept {
  return _asyncPrefetchSlotsManager;
}

}  // namespace arangodb
