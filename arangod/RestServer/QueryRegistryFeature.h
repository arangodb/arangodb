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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "RestServer/arangod.h"
#include "Aql/AsyncPrefetchSlotsManager.h"
#include "Aql/QueryRegistry.h"
#include "Metrics/Fwd.h"
#include "RestServer/QueryRegistryFeatureOptions.h"

#include <atomic>

namespace arangodb {

class QueryRegistryFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "QueryRegistry"; }

  static aql::QueryRegistry* registry() {
    return QUERY_REGISTRY.load(std::memory_order_acquire);
  }

  QueryRegistryFeature(Server& server, metrics::MetricsFeature& metrics);
  ~QueryRegistryFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  void updateMetrics();

  // tracks a query start
  void trackQueryStart() noexcept;
  // tracks a query completion, using execution time
  void trackQueryEnd(double time);
  // tracks a slow query, using execution time
  void trackSlowQuery(double time);

  bool trackingEnabled() const noexcept { return _options.trackingEnabled; }
  bool trackSlowQueries() const noexcept { return _options.trackSlowQueries; }
  bool trackQueryString() const noexcept { return _options.trackQueryString; }
  bool trackBindVars() const noexcept { return _options.trackBindVars; }
  bool trackDataSources() const noexcept { return _options.trackDataSources; }
  double slowQueryThreshold() const noexcept { return _options.slowQueryThreshold; }
  double slowStreamingQueryThreshold() const noexcept {
    return _options.slowStreamingQueryThreshold;
  }
  size_t maxQueryStringLength() const noexcept { return _options.maxQueryStringLength; }
  uint64_t peakMemoryUsageThreshold() const noexcept {
    return _options.peakMemoryUsageThreshold;
  }
  bool failOnWarning() const noexcept { return _options.failOnWarning; }
  bool requireWith() const noexcept { return _options.requireWith; }
#ifdef USE_ENTERPRISE
  bool smartJoins() const noexcept { return _options.smartJoins; }
  bool parallelizeTraversals() const noexcept { return _options.parallelizeTraversals; }
#endif
  size_t maxCollectionsPerQuery() const noexcept {
    return _options.maxCollectionsPerQuery;
  }
  bool allowCollectionsInExpressions() const noexcept {
    return _options.allowCollectionsInExpressions;
  }
  bool logFailedQueries() const noexcept { return _options.logFailedQueries; }
  size_t leaseAsyncPrefetchSlots(size_t value) noexcept;
  void returnAsyncPrefetchSlots(size_t value) noexcept;
  uint64_t queryGlobalMemoryLimit() const noexcept {
    return _options.queryGlobalMemoryLimit;
  }
  uint64_t queryMemoryLimit() const noexcept { return _options.queryMemoryLimit; }
  double queryMaxRuntime() const noexcept { return _options.queryMaxRuntime; }
  uint64_t maxQueryPlans() const noexcept { return _options.maxQueryPlans; }
  aql::QueryRegistry* queryRegistry() const noexcept {
    return _queryRegistry.get();
  }
  uint64_t maxParallelism() const noexcept { return _options.maxParallelism; }

  uint64_t queryPlanCacheMaxEntries() const noexcept {
    return _options.queryPlanCacheMaxEntries;
  }
  uint64_t queryPlanCacheMaxMemoryUsage() const noexcept {
    return _options.queryPlanCacheMaxMemoryUsage;
  }
  uint64_t queryPlanCacheMaxIndividualEntrySize() const noexcept {
    return _options.queryPlanCacheMaxIndividualEntrySize;
  }
  double queryPlanCacheInvalidationTime() const noexcept {
    return _options.queryPlanCacheInvalidationTime;
  }
  metrics::Counter* queryPlanCacheHitsMetric() const {
    return &_queryPlanCacheHitsMetric;
  }
  metrics::Counter* queryPlanCacheMissesMetric() const {
    return &_queryPlanCacheMissesMetric;
  }
  metrics::Gauge<uint64_t>* queryPlanCacheMemoryUsage() const {
    return &_queryPlanCacheMemoryUsage;
  }

  metrics::Gauge<uint64_t>* cursorsMetric() const { return &_activeCursors; }
  metrics::Gauge<uint64_t>* cursorsMemoryUsageMetric() const {
    return &_cursorsMemoryUsage;
  }

  aql::AsyncPrefetchSlotsManager& asyncPrefetchSlotsManager() noexcept;

 private:
  QueryRegistryFeatureOptions _options;

  static std::atomic<aql::QueryRegistry*> QUERY_REGISTRY;

  std::unique_ptr<aql::QueryRegistry> _queryRegistry;
  aql::AsyncPrefetchSlotsManager _asyncPrefetchSlotsManager;

  metrics::Histogram<metrics::LogScale<double>>& _queryTimes;
  metrics::Histogram<metrics::LogScale<double>>& _slowQueryTimes;
  metrics::Counter& _totalQueryExecutionTime;
  metrics::Counter& _queriesCounter;
  metrics::Gauge<uint64_t>& _runningQueries;
  metrics::Gauge<uint64_t>& _globalQueryMemoryUsage;
  metrics::Gauge<uint64_t>& _globalQueryMemoryLimit;
  metrics::Counter& _globalQueryMemoryLimitReached;
  metrics::Counter& _localQueryMemoryLimitReached;
  metrics::Gauge<uint64_t>& _activeCursors;
  metrics::Gauge<uint64_t>& _cursorsMemoryUsage;
  metrics::Counter& _queryPlanCacheHitsMetric;
  metrics::Counter& _queryPlanCacheMissesMetric;
  metrics::Gauge<uint64_t>& _queryPlanCacheMemoryUsage;
};

}  // namespace arangodb
