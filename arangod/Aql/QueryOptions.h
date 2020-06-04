////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_QUERY_OPTIONS_H
#define ARANGOD_AQL_QUERY_OPTIONS_H 1

#include <string>
#include <unordered_set>
#include <vector>

#include "Basics/Common.h"
#include "Transaction/Options.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack
class QueryRegistryFeature;

namespace aql {

enum ProfileLevel : uint32_t {
  /// no profiling information
  PROFILE_LEVEL_NONE = 0,
  /// Output timing for query stages
  PROFILE_LEVEL_BASIC = 1,
  /// Enable instrumentation for getSome calls
  PROFILE_LEVEL_BLOCKS = 2,
  /// Log tracing info for getSome calls
  PROFILE_LEVEL_TRACE_1 = 3,
  /// Log tracing information including getSome results
  PROFILE_LEVEL_TRACE_2 = 4
};

struct QueryOptions {
  explicit QueryOptions(QueryRegistryFeature&);
  TEST_VIRTUAL ~QueryOptions() = default;

  void fromVelocyPack(arangodb::velocypack::Slice const& slice);
  void toVelocyPack(arangodb::velocypack::Builder&, bool disableOptimizerRules) const;
  TEST_VIRTUAL ProfileLevel getProfileLevel() const { return profile; };

  size_t memoryLimit;
  size_t maxNumberOfPlans;
  size_t maxWarningCount;
  double maxRuntime; // query has to execute within the given time or will be killed
  double satelliteSyncWait;
  double ttl; // time until query cursor expires - avoids coursors to
              // stick around for ever if client does not collect the data
  /// Level 0 nothing, Level 1 profile, Level 2,3 log tracing info
  ProfileLevel profile;
  bool allPlans;
  bool verbosePlans;
  bool stream;
  bool silent;
  bool failOnWarning;
  bool cache;
  bool fullCount;
  bool count;
  bool verboseErrors;
  bool inspectSimplePlans;
  
  /// @brief hack to be used only for /_api/export, contains the name of
  /// the target collection
  std::string exportCollection;
  
  /// @brief optimizer rules to turn off/on manually
  std::vector<std::string> optimizerRules;
  
  /// @brief manual restriction to certain shards
  std::unordered_set<std::string> restrictToShards;

#ifdef USE_ENTERPRISE
  // TODO: remove as soon as we have cluster wide transactions
  std::unordered_set<std::string> inaccessibleCollections;
#endif

  transaction::Options transactionOptions;
};

}  // namespace aql
}  // namespace arangodb

#endif
