////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "ShardingFeature.h"
#include "Cluster/ServerState.h"
#include "Cluster/ShardingInfo.h"
#include "Cluster/ShardingStrategyDefault.h"
#include "VocBase/LogicalCollection.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Cluster/ShardingStrategyEE.h"
#endif

#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;

ShardingFeature::ShardingFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Sharding") {
  setOptional(true);
  startsAfter("Logger");
  startsBefore("Cluster");
}

void ShardingFeature::prepare() {
  registerFactory(ShardingStrategyNone::NAME, [](ShardingInfo*) { 
    return std::make_unique<ShardingStrategyNone>();
  });
  registerFactory(ShardingStrategyCommunityCompat::NAME, [](ShardingInfo* sharding) { 
    return std::make_unique<ShardingStrategyCommunityCompat>(sharding);
  });
#ifdef USE_ENTERPRISE
  registerFactory(ShardingStrategyEnterpriseCompat::NAME, [](ShardingInfo* sharding) { 
    return std::make_unique<ShardingStrategyEnterpriseCompat>(sharding);
  });
  registerFactory(ShardingStrategyEnterpriseSmartEdgeCompat::NAME, [](ShardingInfo* sharding) {
    return std::make_unique<ShardingStrategyEnterpriseSmartEdgeCompat>(sharding);
  });
#endif
}
  
void ShardingFeature::registerFactory(std::string const& name, 
                                      ShardingStrategy::FactoryFunction const& creator) {
  LOG_TOPIC(TRACE, Logger::CLUSTER) << "registering sharding strategy '" << name << "'";

  if (!_factories.emplace(name, creator).second) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("sharding factory function '") + name + "' already registered");
  }
}

std::unique_ptr<ShardingStrategy> ShardingFeature::fromVelocyPack(VPackSlice slice, 
                                                                  ShardingInfo* sharding) {
  if (!slice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, "invalid collection meta data");
  }

  std::string name;

  if (!ServerState::instance()->isRunningInCluster()) {
    // not running in cluster... so no sharding
    name = ShardingStrategyNone::NAME;
  } else {
    // running in cluster... determine the correct method for sharding
    VPackSlice s = slice.get("shardingStrategy");
    if (s.isString()) {
      name = s.copyString();
    } else {
      name = getDefaultShardingStrategy(sharding);
    }
  }

  return create(name, sharding);
}

std::unique_ptr<ShardingStrategy> ShardingFeature::create(std::string const& name, ShardingInfo* sharding) {
  auto it = _factories.find(name);
  
  if (it == _factories.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("unknown sharding type '") + name + "'");
  }

  // now create a sharding strategy instance
  return (*it).second(sharding);
}

std::string ShardingFeature::getDefaultShardingStrategy(ShardingInfo const* sharding) const {
  TRI_ASSERT(ServerState::instance()->isRunningInCluster());
  
  if (ServerState::instance()->isDBServer()) {
    // on a DB server, we will not use sharding
    return ShardingStrategyNone::NAME;
  }

  // no sharding strategy found in collection meta data
#ifdef USE_ENTERPRISE
  if (sharding->collection()->isSmart() && 
      sharding->collection()->type() == TRI_COL_TYPE_EDGE) {
    // smart edge collection
    return ShardingStrategyEnterpriseSmartEdgeCompat::NAME;
  }
   
  return ShardingStrategyEnterpriseCompat::NAME;
#else
  return ShardingStrategyCommunityCompat::NAME;
#endif 
}
