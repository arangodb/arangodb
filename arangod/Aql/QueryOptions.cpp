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

#include "QueryOptions.h"

#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "Basics/StaticStrings.h"
#include "Basics/debugging.h"
#include "Cluster/ServerState.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb::aql;

size_t QueryOptions::defaultMemoryLimit = 0U;
size_t QueryOptions::defaultMaxNumberOfPlans = 128U;
size_t QueryOptions::defaultMaxNodesPerCallstack = 250U;
size_t QueryOptions::defaultSpillOverThresholdNumRows = 5000000ULL;
size_t QueryOptions::defaultSpillOverThresholdMemoryUsage =
    134217728ULL;                                                // 128 MB
size_t QueryOptions::defaultMaxDNFConditionMembers = 786432ULL;  // 768K
double QueryOptions::defaultMaxRuntime = 0.0;
double QueryOptions::defaultTtl;
bool QueryOptions::defaultFailOnWarning = false;
bool QueryOptions::allowMemoryLimitOverride = true;

QueryOptions::QueryOptions()
    : memoryLimit(0),
      maxNumberOfPlans(QueryOptions::defaultMaxNumberOfPlans),
      maxWarningCount(10),
      maxNodesPerCallstack(QueryOptions::defaultMaxNodesPerCallstack),
      spillOverThresholdNumRows(QueryOptions::defaultSpillOverThresholdNumRows),
      spillOverThresholdMemoryUsage(
          QueryOptions::defaultSpillOverThresholdMemoryUsage),
      maxDNFConditionMembers(QueryOptions::defaultMaxDNFConditionMembers),
      maxRuntime(0.0),
#ifdef USE_ENTERPRISE
      satelliteSyncWait(std::chrono::seconds(60)),
#endif
      // get global default ttl
      ttl(QueryOptions::defaultTtl),
      profile(ProfileLevel::None),
      traversalProfile(TraversalProfileLevel::None),
      allPlans(false),
      verbosePlans(false),
      explainInternals(true),
      stream(false),
      retriable(false),
      silent(false),
      // use global "failOnWarning" value
      failOnWarning(QueryOptions::defaultFailOnWarning),
      cache(false),
      fullCount(false),
      count(false),
      skipAudit(false),
      optimizePlanForCaching(false),
      explainRegisters(ExplainRegisterPlan::No),
      desiredJoinStrategy(JoinStrategyType::kDefault) {
  // now set some default values from server configuration options
  // use global memory limit value
  if (uint64_t globalLimit = QueryOptions::defaultMemoryLimit;
      globalLimit > 0) {
    memoryLimit = globalLimit;
  }

  // use global max runtime value
  if (double globalLimit = QueryOptions::defaultMaxRuntime; globalLimit > 0.0) {
    maxRuntime = globalLimit;
  }

  // "cache" only defaults to true if query cache is turned on
  cache = (QueryCache::instance()->mode() == CACHE_ALWAYS_ON);

  TRI_ASSERT(maxNumberOfPlans > 0);
}

QueryOptions::QueryOptions(velocypack::Slice slice) : QueryOptions() {
  this->fromVelocyPack(slice);
}

void QueryOptions::fromVelocyPack(VPackSlice slice) {
  if (!slice.isObject()) {
    return;
  }

  // use global memory limit value first
  if (QueryOptions::defaultMemoryLimit > 0) {
    memoryLimit = QueryOptions::defaultMemoryLimit;
  }

  // numeric options
  if (VPackSlice value = slice.get("memoryLimit"); value.isNumber()) {
    size_t v = value.getNumber<size_t>();
    if (allowMemoryLimitOverride) {
      memoryLimit = v;
    } else if (v > 0 && v < memoryLimit) {
      // only allow increasing the memory limit if the respective startup option
      // is set. and if it is not set, only allow decreasing the memory limit
      memoryLimit = v;
    }
  }

  if (VPackSlice value = slice.get("maxNumberOfPlans"); value.isNumber()) {
    maxNumberOfPlans = value.getNumber<size_t>();
    if (maxNumberOfPlans == 0) {
      maxNumberOfPlans = 1;
    }
  }

  if (VPackSlice value = slice.get("maxWarningCount"); value.isNumber()) {
    maxWarningCount = value.getNumber<size_t>();
  }

  if (VPackSlice value = slice.get("maxNodesPerCallstack"); value.isNumber()) {
    maxNodesPerCallstack = value.getNumber<size_t>();
  }

  if (VPackSlice value = slice.get("spillOverThresholdNumRows");
      value.isNumber()) {
    spillOverThresholdNumRows = value.getNumber<size_t>();
  }

  if (VPackSlice value = slice.get("spillOverThresholdMemoryUsage");
      value.isNumber()) {
    spillOverThresholdMemoryUsage = value.getNumber<size_t>();
  }

  if (VPackSlice value = slice.get("maxDNFConditionMembers");
      value.isNumber()) {
    maxDNFConditionMembers = value.getNumber<size_t>();
  }

  if (VPackSlice value = slice.get("maxRuntime"); value.isNumber()) {
    maxRuntime = value.getNumber<double>();
  }

#ifdef USE_ENTERPRISE
  if (VPackSlice value = slice.get("satelliteSyncWait"); value.isNumber()) {
    satelliteSyncWait =
        std::chrono::duration<double>(value.getNumber<double>());
  }
#endif

  if (VPackSlice value = slice.get("ttl"); value.isNumber()) {
    ttl = value.getNumber<double>();
  }

  // boolean options
  if (VPackSlice value = slice.get("profile"); value.isBool()) {
    profile = value.getBool() ? ProfileLevel::Basic : ProfileLevel::None;
  } else if (value.isNumber()) {
    profile = static_cast<ProfileLevel>(value.getNumber<uint16_t>());
  }

  if (VPackSlice value = slice.get(StaticStrings::GraphTraversalProfileLevel);
      value.isBool()) {
    traversalProfile = value.getBool() ? TraversalProfileLevel::Basic
                                       : TraversalProfileLevel::None;
  } else if (value.isNumber()) {
    traversalProfile =
        static_cast<TraversalProfileLevel>(value.getNumber<uint16_t>());
  }

  if (VPackSlice value = slice.get("allPlans"); value.isBool()) {
    allPlans = value.isTrue();
  }
  if (VPackSlice value = slice.get("verbosePlans"); value.isBool()) {
    verbosePlans = value.isTrue();
  }
  if (VPackSlice value = slice.get("explainInternals"); value.isBool()) {
    explainInternals = value.isTrue();
  }
  if (VPackSlice value = slice.get("stream"); value.isBool()) {
    stream = value.isTrue();
  }
  if (VPackSlice value = slice.get("allowRetry"); value.isBool()) {
    retriable = value.isTrue();
  }
  if (VPackSlice value = slice.get("silent"); value.isBool()) {
    silent = value.isTrue();
  }
  if (VPackSlice value = slice.get("failOnWarning"); value.isBool()) {
    failOnWarning = value.isTrue();
  }
  if (VPackSlice value = slice.get("cache"); value.isBool()) {
    cache = value.isTrue();
  }
  if (VPackSlice value = slice.get("optimizePlanForCaching"); value.isBool()) {
    optimizePlanForCaching = value.isTrue();
  }
  if (VPackSlice value = slice.get("fullCount"); value.isBool()) {
    fullCount = value.isTrue();
  }
  if (VPackSlice value = slice.get("count"); value.isBool()) {
    count = value.isTrue();
  }
  if (VPackSlice value = slice.get("explainRegisters"); value.isBool()) {
    explainRegisters =
        value.isTrue() ? ExplainRegisterPlan::Yes : ExplainRegisterPlan::No;
  }

  // note: skipAudit is intentionally not read here.
  // the end user cannot override this setting

  if (VPackSlice value = slice.get(StaticStrings::ForceOneShardAttributeValue);
      value.isString()) {
    forceOneShardAttributeValue = value.copyString();
  }

  if (VPackSlice value = slice.get(StaticStrings::JoinStrategyType);
      value.isString()) {
    if (value.stringView() == "generic") {
      desiredJoinStrategy = JoinStrategyType::kGeneric;
    }
  }

  if (VPackSlice optimizer = slice.get("optimizer"); optimizer.isObject()) {
    if (VPackSlice value = optimizer.get("rules"); value.isArray()) {
      for (auto rule : VPackArrayIterator(value)) {
        if (rule.isString()) {
          optimizerRules.emplace_back(rule.copyString());
        }
      }
    }
  }
  // On single server we cannot restrict to shards.
  if (!ServerState::instance()->isSingleServer()) {
    if (VPackSlice value = slice.get("shardIds"); value.isArray()) {
      for (auto id : VPackArrayIterator(value)) {
        if (id.isString()) {
          restrictToShards.emplace(id.copyString());
        }
      }
    }
  }

#ifdef USE_ENTERPRISE
  if (VPackSlice value = slice.get("inaccessibleCollections");
      value.isArray()) {
    for (auto c : VPackArrayIterator(value)) {
      if (c.isString()) {
        inaccessibleCollections.emplace(c.copyString());
      }
    }
  }
#endif

  // also handle transaction options
  transactionOptions.fromVelocyPack(slice);
}

void QueryOptions::toVelocyPack(VPackBuilder& builder,
                                bool disableOptimizerRules) const {
  builder.openObject();

  builder.add("memoryLimit", VPackValue(memoryLimit));
  builder.add("maxNumberOfPlans", VPackValue(maxNumberOfPlans));
  builder.add("maxWarningCount", VPackValue(maxWarningCount));
  builder.add("maxNodesPerCallstack", VPackValue(maxNodesPerCallstack));
  builder.add("spillOverThresholdNumRows",
              VPackValue(spillOverThresholdNumRows));
  builder.add("spillOverThresholdMemoryUsage",
              VPackValue(spillOverThresholdMemoryUsage));
  builder.add("maxDNFConditionMembers", VPackValue(maxDNFConditionMembers));
  builder.add("maxRuntime", VPackValue(maxRuntime));
#ifdef USE_ENTERPRISE
  builder.add("satelliteSyncWait", VPackValue(satelliteSyncWait.count()));
#endif
  builder.add("ttl", VPackValue(ttl));
  builder.add("profile", VPackValue(static_cast<uint32_t>(profile)));
  builder.add(StaticStrings::GraphTraversalProfileLevel,
              VPackValue(static_cast<uint32_t>(traversalProfile)));
  builder.add("allPlans", VPackValue(allPlans));
  builder.add("verbosePlans", VPackValue(verbosePlans));
  builder.add("explainInternals", VPackValue(explainInternals));
  builder.add("stream", VPackValue(stream));
  builder.add("allowRetry", VPackValue(retriable));
  builder.add("silent", VPackValue(silent));
  builder.add("failOnWarning", VPackValue(failOnWarning));
  builder.add("cache", VPackValue(cache));
  builder.add("optimizePlanForCaching", VPackValue(optimizePlanForCaching));
  builder.add("fullCount", VPackValue(fullCount));
  builder.add("count", VPackValue(count));

  if (desiredJoinStrategy == JoinStrategyType::kGeneric) {
    builder.add(StaticStrings::JoinStrategyType, VPackValue("generic"));
  } else {
    TRI_ASSERT(desiredJoinStrategy == JoinStrategyType::kDefault);
    builder.add(StaticStrings::JoinStrategyType, VPackValue("default"));
  }

  if (!forceOneShardAttributeValue.empty()) {
    builder.add(StaticStrings::ForceOneShardAttributeValue,
                VPackValue(forceOneShardAttributeValue));
  }

  // note: skipAudit is intentionally not serialized here.
  // the end user cannot override this setting anyway.

  builder.add("optimizer", VPackValue(VPackValueType::Object));
  if (!optimizerRules.empty() || disableOptimizerRules) {
    builder.add("rules", VPackValue(VPackValueType::Array));
    if (disableOptimizerRules) {
      // turn off all optimizer rules
      builder.add(VPackValue("-all"));
    } else {
      for (auto const& it : optimizerRules) {
        builder.add(VPackValue(it));
      }
    }
    builder.close();  // optimizer.rules
  }
  builder.close();  // optimizer

  if (!restrictToShards.empty()) {
    builder.add("shardIds", VPackValue(VPackValueType::Array));
    for (auto const& it : restrictToShards) {
      builder.add(VPackValue(it));
    }
    builder.close();  // shardIds
  }

#ifdef USE_ENTERPRISE
  if (!inaccessibleCollections.empty()) {
    builder.add("inaccessibleCollections", VPackValue(VPackValueType::Array));
    for (auto const& it : inaccessibleCollections) {
      builder.add(VPackValue(it));
    }
    builder.close();  // inaccessibleCollections
  }
#endif

  // also handle transaction options
  transactionOptions.toVelocyPack(builder);

  builder.close();
}
