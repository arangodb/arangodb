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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ProfileLevel.h"
#include "Aql/types.h"
#include "Cluster/Utils/ShardID.h"
#include "Transaction/Options.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_set>
#include <vector>

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {

struct QueryOptions {
  QueryOptions();
  explicit QueryOptions(velocypack::Slice);
  QueryOptions(QueryOptions&&) noexcept = default;
  QueryOptions(QueryOptions const&) = default;
  TEST_VIRTUAL ~QueryOptions() = default;

  enum JoinStrategyType : uint8_t { kDefault, kGeneric };

  void fromVelocyPack(velocypack::Slice slice);
  void toVelocyPack(velocypack::Builder& builder,
                    bool disableOptimizerRules) const;
  TEST_VIRTUAL ProfileLevel getProfileLevel() const { return profile; }
  TEST_VIRTUAL TraversalProfileLevel getTraversalProfileLevel() const {
    return traversalProfile;
  }

  size_t memoryLimit;
  size_t maxNumberOfPlans;
  size_t maxWarningCount;
  size_t maxNodesPerCallstack;
  size_t spillOverThresholdNumRows;
  size_t spillOverThresholdMemoryUsage;
  size_t maxDNFConditionMembers;
  // query has to execute within the given time or will be killed
  double maxRuntime;

#ifdef USE_ENTERPRISE
  std::chrono::duration<double> satelliteSyncWait;
#endif

  double ttl;  // time until query cursor expires - avoids coursors to
               // stick around for ever if client does not collect the data
  /// Level 0 nothing, Level 1 profile, Level 2,3 log tracing info
  ProfileLevel profile;
  TraversalProfileLevel traversalProfile;
  // make explain return all generated query executed plans
  bool allPlans;
  // add more detail to query execution plans
  bool verbosePlans;
  // add even more detail (internals) to query execution plans
  bool explainInternals;
  bool stream;
  bool retriable;
  // do not return query results
  bool silent;
  // make the query fail if a warning is produced
  bool failOnWarning;
  // whether or not the query result is allowed to be stored in the
  // query results cache
  bool cache;
  // whether or not the fullCount should be returned
  bool fullCount;
  bool count;
  // skips audit logging - used only internally
  bool skipAudit;
  // whether or not the optimizer result should be cached
  bool optimizePlanForCaching;

  ExplainRegisterPlan explainRegisters;

  /// @brief desired join strategy used by the JoinNode (if available)
  JoinStrategyType desiredJoinStrategy;

  /// @brief shard key attribute value used to push a query down
  /// to a single server
  std::string forceOneShardAttributeValue;

  /// @brief optimizer rules to turn off/on manually
  std::vector<std::string> optimizerRules;

  /// @brief manual restriction to certain shards
  std::unordered_set<ShardID> restrictToShards;

#ifdef USE_ENTERPRISE
  // TODO: remove as soon as we have cluster wide transactions
  std::unordered_set<std::string> inaccessibleCollections;
#endif

  transaction::Options transactionOptions;

  static size_t defaultMemoryLimit;
  static size_t defaultMaxNumberOfPlans;
  static size_t defaultMaxNodesPerCallstack;
  static size_t defaultSpillOverThresholdNumRows;
  static size_t defaultSpillOverThresholdMemoryUsage;
  static size_t defaultMaxDNFConditionMembers;
  static double defaultMaxRuntime;
  static double defaultTtl;
  static bool defaultFailOnWarning;
  static bool allowMemoryLimitOverride;
};

}  // namespace aql
}  // namespace arangodb
