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

#include <cstddef>
#include <cstdint>
#include <string>

namespace arangodb {

struct QueryRegistryFeatureOptions {
  bool trackingEnabled;
  bool trackSlowQueries;
  bool trackQueryString;
  bool trackBindVars;
  bool trackDataSources;
  bool failOnWarning;
  bool requireWith;
  bool queryCacheIncludeSystem;
  bool queryMemoryLimitOverride;
#ifdef USE_ENTERPRISE
  bool smartJoins;
  bool parallelizeTraversals;
#endif
  bool allowCollectionsInExpressions;
  bool logFailedQueries;
  size_t maxAsyncPrefetchSlotsTotal;
  size_t maxAsyncPrefetchSlotsPerQuery;
  size_t maxQueryStringLength;
  size_t maxCollectionsPerQuery;
  uint64_t peakMemoryUsageThreshold;
  uint64_t queryGlobalMemoryLimit;
  uint64_t queryMemoryLimit;
  size_t maxDNFConditionMembers;
  double queryMaxRuntime;
  uint64_t maxQueryPlans;
  uint64_t maxNodesPerCallstack;
  uint64_t queryPlanCacheMaxEntries;
  uint64_t queryPlanCacheMaxMemoryUsage;
  uint64_t queryPlanCacheMaxIndividualEntrySize;
  double queryPlanCacheInvalidationTime;
  uint64_t queryCacheMaxResultsCount;
  uint64_t queryCacheMaxResultsSize;
  uint64_t queryCacheMaxEntrySize;
  uint64_t maxParallelism;
  double slowQueryThreshold;
  double slowStreamingQueryThreshold;
  double queryRegistryTTL;
  std::string queryCacheMode;
};

}  // namespace arangodb
