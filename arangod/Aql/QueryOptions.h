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

#include "Basics/Common.h"
#include "Transaction/Options.h"

namespace arangodb {
namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

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
  QueryOptions();

  void fromVelocyPack(arangodb::velocypack::Slice const&);
  void toVelocyPack(arangodb::velocypack::Builder&, bool disableOptimizerRules) const;

  size_t memoryLimit;
  size_t maxNumberOfPlans;
  size_t maxWarningCount;
  int64_t literalSizeThreshold;
  double satelliteSyncWait;
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
  std::vector<std::string> optimizerRules;
  std::unordered_set<std::string> shardIds;
#ifdef USE_ENTERPRISE
  // TODO: remove as soon as we have cluster wide transactions
  std::unordered_set<std::string> inaccessibleCollections;
#endif

  transaction::Options transactionOptions;
};

}  // namespace aql
}  // namespace arangodb

#endif
