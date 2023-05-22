////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "RestServer/arangod.h"
#include "Aql/QueryRegistry.h"
#include "Metrics/Fwd.h"

namespace arangodb {

class QueryRegistryFeature final : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept { return "QueryRegistry"; }

  static aql::QueryRegistry* registry() {
    return QUERY_REGISTRY.load(std::memory_order_acquire);
  }

  explicit QueryRegistryFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
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

  bool trackingEnabled() const noexcept { return _trackingEnabled; }
  bool trackSlowQueries() const noexcept { return _trackSlowQueries; }
  bool trackQueryString() const noexcept { return _trackQueryString; }
  bool trackBindVars() const noexcept { return _trackBindVars; }
  bool trackDataSources() const noexcept { return _trackDataSources; }
  double slowQueryThreshold() const noexcept { return _slowQueryThreshold; }
  double slowStreamingQueryThreshold() const noexcept {
    return _slowStreamingQueryThreshold;
  }
  size_t maxQueryStringLength() const noexcept { return _maxQueryStringLength; }
  uint64_t peakMemoryUsageThreshold() const noexcept {
    return _peakMemoryUsageThreshold;
  }
  bool failOnWarning() const noexcept { return _failOnWarning; }
  bool requireWith() const noexcept { return _requireWith; }
#ifdef USE_ENTERPRISE
  bool smartJoins() const noexcept { return _smartJoins; }
  size_t maxCollectionsPerQuery() const noexcept {
    return _maxCollectionsPerQuery;
  }
  bool parallelizeTraversals() const noexcept { return _parallelizeTraversals; }
#endif
  bool allowCollectionsInExpressions() const noexcept {
    return _allowCollectionsInExpressions;
  }
  bool logFailedQueries() const noexcept { return _logFailedQueries; }
  uint64_t queryGlobalMemoryLimit() const noexcept {
    return _queryGlobalMemoryLimit;
  }
  uint64_t queryMemoryLimit() const noexcept { return _queryMemoryLimit; }
  double queryMaxRuntime() const noexcept { return _queryMaxRuntime; }
  uint64_t maxQueryPlans() const noexcept { return _maxQueryPlans; }
  aql::QueryRegistry* queryRegistry() const noexcept {
    return _queryRegistry.get();
  }
  uint64_t maxParallelism() const noexcept { return _maxParallelism; }

 private:
  bool _trackingEnabled;
  bool _trackSlowQueries;
  bool _trackQueryString;
  bool _trackBindVars;
  bool _trackDataSources;
  bool _failOnWarning;
  bool _requireWith;
  bool _queryCacheIncludeSystem;
  bool _queryMemoryLimitOverride;
#ifdef USE_ENTERPRISE
  bool _smartJoins;
  bool _parallelizeTraversals;
#endif
  bool _allowCollectionsInExpressions;
  bool _logFailedQueries;
  size_t _maxQueryStringLength;
  size_t _maxCollectionsPerQuery;
  uint64_t _peakMemoryUsageThreshold;
  uint64_t _queryGlobalMemoryLimit;
  uint64_t _queryMemoryLimit;
  size_t _maxDNFConditionMembers;
  double _queryMaxRuntime;
  uint64_t _maxQueryPlans;
  uint64_t _maxNodesPerCallstack;
  uint64_t _queryCacheMaxResultsCount;
  uint64_t _queryCacheMaxResultsSize;
  uint64_t _queryCacheMaxEntrySize;
  uint64_t _maxParallelism;
  double _slowQueryThreshold;
  double _slowStreamingQueryThreshold;
  double _queryRegistryTTL;
  std::string _queryCacheMode;

 private:
  static std::atomic<aql::QueryRegistry*> QUERY_REGISTRY;

  std::unique_ptr<aql::QueryRegistry> _queryRegistry;

  metrics::Histogram<metrics::LogScale<double>>& _queryTimes;
  metrics::Histogram<metrics::LogScale<double>>& _slowQueryTimes;
  metrics::Counter& _totalQueryExecutionTime;
  metrics::Counter& _queriesCounter;
  metrics::Gauge<uint64_t>& _runningQueries;
  metrics::Gauge<uint64_t>& _globalQueryMemoryUsage;
  metrics::Gauge<uint64_t>& _globalQueryMemoryLimit;
  metrics::Counter& _globalQueryMemoryLimitReached;
  metrics::Counter& _localQueryMemoryLimitReached;
};

}  // namespace arangodb
