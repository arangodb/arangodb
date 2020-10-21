////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef APPLICATION_FEATURES_QUERY_REGISTRY_FEATUREx_H
#define APPLICATION_FEATURES_QUERY_REGISTRY_FEATUREx_H 1

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Aql/QueryRegistry.h"
#include "RestServer/Metrics.h"

namespace arangodb {

class QueryRegistryFeature final : public application_features::ApplicationFeature {
 public:
  static aql::QueryRegistry* registry() {
    return QUERY_REGISTRY.load(std::memory_order_acquire);
  }

  explicit QueryRegistryFeature(application_features::ApplicationServer& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  void beginShutdown() override final;
  void stop() override final;
  void unprepare() override final;

  // tracks a query, using execution time
  void trackQuery(double time);
  // tracks a slow query, using execution time
  void trackSlowQuery(double time);

  bool trackingEnabled() const { return _trackingEnabled; }
  bool trackSlowQueries() const { return _trackSlowQueries; }
  bool trackQueryString() const { return _trackQueryString; }
  bool trackBindVars() const { return _trackBindVars; }
  bool trackDataSources() const { return _trackDataSources; }
  double slowQueryThreshold() const { return _slowQueryThreshold; }
  double slowStreamingQueryThreshold() const {
    return _slowStreamingQueryThreshold;
  }
  bool failOnWarning() const { return _failOnWarning; }
#ifdef USE_ENTERPRISE
  bool smartJoins() const { return _smartJoins; }
  bool parallelizeTraversals() const { return _parallelizeTraversals; }
#endif
  uint64_t queryMemoryLimit() const { return _queryMemoryLimit; }
  double queryMaxRuntime() const { return _queryMaxRuntime; }
  uint64_t maxQueryPlans() const { return _maxQueryPlans; }
  aql::QueryRegistry* queryRegistry() const { return _queryRegistry.get(); }
  uint64_t maxParallelism() const { return _maxParallelism; }

 private:
  bool _trackingEnabled;
  bool _trackSlowQueries;
  bool _trackQueryString;
  bool _trackBindVars;
  bool _trackDataSources;
  bool _failOnWarning;
  bool _queryCacheIncludeSystem;
#ifdef USE_ENTERPRISE
  bool _smartJoins;
  bool _parallelizeTraversals;
#endif
  uint64_t _queryMemoryLimit;
  double _queryMaxRuntime;
  uint64_t _maxQueryPlans;
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

  Histogram<log_scale_t<double>>& _queryTimes;
  Histogram<log_scale_t<double>>& _slowQueryTimes;
  Counter& _totalQueryExecutionTime;
  Counter& _queriesCounter;
  Counter& _slowQueriesCounter;
};

}  // namespace arangodb

#endif
