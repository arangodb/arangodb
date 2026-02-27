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

#pragma once

#include "Aql/QueryOptions.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace arangodb {

struct QueryRegistryFeatureOptions {
  bool trackingEnabled = true;
  bool trackSlowQueries = true;
  bool trackQueryString = true;
  bool trackBindVars = true;
  bool trackDataSources = false;
  bool failOnWarning = aql::QueryOptions::defaultInitialFailOnWarning;
  bool requireWith = false;
  bool queryCacheIncludeSystem = false;
  bool queryMemoryLimitOverride = true;
#ifdef USE_ENTERPRISE
  bool smartJoins = true;
  bool parallelizeTraversals = true;
#endif
  bool logFailedQueries = false;
  size_t maxAsyncPrefetchSlotsTotal = 256;
  size_t maxAsyncPrefetchSlotsPerQuery = 32;
  size_t maxQueryStringLength = 4096;
  size_t maxCollectionsPerQuery = 2048;
  uint64_t peakMemoryUsageThreshold = 1073741824;  // 1GB
  uint64_t queryGlobalMemoryLimit = 0;
  uint64_t queryMemoryLimit = 0;
  size_t maxDNFConditionMembers =
      aql::QueryOptions::defaultInitialMaxDNFConditionMembers;
  double queryMaxRuntime = aql::QueryOptions::defaultInitialMaxRuntime;
  uint64_t maxQueryPlans = aql::QueryOptions::defaultInitialMaxNumberOfPlans;
  uint64_t maxNodesPerCallstack =
      aql::QueryOptions::defaultInitialMaxNodesPerCallstack;
  uint64_t queryPlanCacheMaxEntries = 128;
  uint64_t queryPlanCacheMaxMemoryUsage = 8 * 1024 * 1024;
  uint64_t queryPlanCacheMaxIndividualEntrySize = 2 * 1024 * 1024;
  double queryPlanCacheInvalidationTime = 900.0;
  uint64_t queryCacheMaxResultsCount = 0;
  uint64_t queryCacheMaxResultsSize = 0;
  uint64_t queryCacheMaxEntrySize = 0;
  uint64_t maxParallelism = 4;
  double slowQueryThreshold = 10.0;
  double slowStreamingQueryThreshold = 10.0;
  double queryRegistryTTL = 0.0;
  std::string queryCacheMode = "off";

  QueryRegistryFeatureOptions();
};

}  // namespace arangodb
