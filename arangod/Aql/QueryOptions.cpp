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

#include "QueryOptions.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/QueryCache.h"
#include "Aql/QueryRegistry.h"
#include "RestServer/QueryRegistryFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

QueryOptions::QueryOptions(arangodb::QueryRegistryFeature& feature)
    : memoryLimit(0),
      maxNumberOfPlans(0),
      maxWarningCount(10),
      maxRuntime(0.0),
      satelliteSyncWait(60.0),
      ttl(0),
      profile(PROFILE_LEVEL_NONE),
      allPlans(false),
      verbosePlans(false),
      stream(false),
      silent(false),
      failOnWarning(false),
      cache(false),
      fullCount(false),
      count(false),
      verboseErrors(false),
      inspectSimplePlans(true) {
  // now set some default values from server configuration options
  {
    // use global memory limit value
    uint64_t globalLimit = feature.queryMemoryLimit();
    if (globalLimit > 0) {
      memoryLimit = globalLimit;
    }
  }

  {
    // use global max runtime value
    double globalLimit = feature.queryMaxRuntime();
    if (globalLimit > 0.0) {
      maxRuntime = globalLimit;
    }
  }

  // get global default ttl
  ttl = feature.registry()->defaultTTL();

  // use global "failOnWarning" value
  failOnWarning = feature.failOnWarning();

  // "cache" only defaults to true if query cache is turned on
  auto queryCacheMode = QueryCache::instance()->mode();
  cache = (queryCacheMode == CACHE_ALWAYS_ON);

  maxNumberOfPlans = feature.maxQueryPlans();
  TRI_ASSERT(maxNumberOfPlans > 0);
}

void QueryOptions::fromVelocyPack(VPackSlice const& slice) {
  if (!slice.isObject()) {
    return;
  }

  VPackSlice value;
  
  // numeric options
  value = slice.get("memoryLimit");
  if (value.isNumber()) {
    size_t v = value.getNumber<size_t>();
    if (v > 0) {
      memoryLimit = v;
    }
  }
  value = slice.get("maxNumberOfPlans");
  if (value.isNumber()) {
    maxNumberOfPlans = value.getNumber<size_t>();
    if (maxNumberOfPlans == 0) {
      maxNumberOfPlans = 1;
    }
  }

  value = slice.get("maxWarningCount");
  if (value.isNumber()) {
    maxWarningCount = value.getNumber<size_t>();
  }

  value = slice.get("maxRuntime");
  if (value.isNumber()) {
    maxRuntime = value.getNumber<double>();
  }


  value = slice.get("satelliteSyncWait");
  if (value.isNumber()) {
    satelliteSyncWait = value.getNumber<double>();
  }

  value = slice.get("ttl");
  if (value.isNumber()) {
    ttl = value.getNumber<double>();
  }

  // boolean options
  value = slice.get("profile");
  if (value.isBool()) {
    profile = value.getBool() ? PROFILE_LEVEL_BASIC : PROFILE_LEVEL_NONE;
  } else if (value.isNumber()) {
    profile = static_cast<ProfileLevel>(value.getNumber<uint32_t>());
  }

  value = slice.get("stream");
  if (value.isBool()) {
    stream = value.getBool();
  }

  value = slice.get("allPlans");
  if (value.isBool()) {
    allPlans = value.getBool();
  }
  value = slice.get("verbosePlans");
  if (value.isBool()) {
    verbosePlans = value.getBool();
  }
  value = slice.get("stream");
  if (value.isBool()) {
    stream = value.getBool();
  }
  value = slice.get("silent");
  if (value.isBool()) {
    silent = value.getBool();
  }
  value = slice.get("failOnWarning");
  if (value.isBool()) {
    failOnWarning = value.getBool();
  }
  value = slice.get("cache");
  if (value.isBool()) {
    cache = value.getBool();
  }
  value = slice.get("fullCount");
  if (value.isBool()) {
    fullCount = value.getBool();
  }
  value = slice.get("count");
  if (value.isBool()) {
    count = value.getBool();
  }
  value = slice.get("verboseErrors");
  if (value.isBool()) {
    verboseErrors = value.getBool();
  }

  VPackSlice optimizer = slice.get("optimizer");
  if (optimizer.isObject()) {
    value = optimizer.get("inspectSimplePlans");
    if (value.isBool()) {
      inspectSimplePlans = value.getBool();
    }
    value = optimizer.get("rules");
    if (value.isArray()) {
      for (auto const& rule : VPackArrayIterator(value)) {
        if (rule.isString()) {
          optimizerRules.emplace_back(rule.copyString());
        }
      }
    }
  }
  value = slice.get("shardIds");
  if (value.isArray()) {
    VPackArrayIterator it(value);
    while (it.valid()) {
      VPackSlice value = it.value();
      if (value.isString()) {
        restrictToShards.emplace(value.copyString());
      }
      it.next();
    }
  }

#ifdef USE_ENTERPRISE
  value = slice.get("inaccessibleCollections");
  if (value.isArray()) {
    VPackArrayIterator it(value);
    while (it.valid()) {
      VPackSlice value = it.value();
      if (value.isString()) {
        inaccessibleCollections.emplace(value.copyString());
      }
      it.next();
    }
  }
#endif
  
  value = slice.get("exportCollection");
  if (value.isString()) {
    exportCollection = value.copyString();
  }

  // also handle transaction options
  transactionOptions.fromVelocyPack(slice);
}

void QueryOptions::toVelocyPack(VPackBuilder& builder, bool disableOptimizerRules) const {
  builder.openObject();

  builder.add("memoryLimit", VPackValue(memoryLimit));
  builder.add("maxNumberOfPlans", VPackValue(maxNumberOfPlans));
  builder.add("maxWarningCount", VPackValue(maxWarningCount));
  builder.add("maxRuntime", VPackValue(maxRuntime));
  builder.add("satelliteSyncWait", VPackValue(satelliteSyncWait));
  builder.add("ttl", VPackValue(ttl));
  builder.add("profile", VPackValue(static_cast<uint32_t>(profile)));
  builder.add("allPlans", VPackValue(allPlans));
  builder.add("verbosePlans", VPackValue(verbosePlans));
  builder.add("stream", VPackValue(stream));
  builder.add("silent", VPackValue(silent));
  builder.add("failOnWarning", VPackValue(failOnWarning));
  builder.add("cache", VPackValue(cache));
  builder.add("fullCount", VPackValue(fullCount));
  builder.add("count", VPackValue(count));
  builder.add("verboseErrors", VPackValue(verboseErrors));

  builder.add("optimizer", VPackValue(VPackValueType::Object));
  builder.add("inspectSimplePlans", VPackValue(inspectSimplePlans));
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
  
  // "exportCollection" is only used internally and not exposed via toVelocyPack

  // also handle transaction options
  transactionOptions.toVelocyPack(builder);

  builder.close();
}
