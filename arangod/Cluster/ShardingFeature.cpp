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
#include "Cluster/ShardingStrategyDefault.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/Cluster/ShardingStrategyEE.h"
#endif

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

ShardingFeature::ShardingFeature(application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Sharding") {
  setOptional(true);
  startsAfter("Logger");
  startsBefore("Cluster");
}

void ShardingFeature::prepare() {
  registerFactory(ShardingStrategyNone::NAME, [](LogicalCollection*) { 
    return std::make_unique<ShardingStrategyNone>();
  });
  registerFactory(ShardingStrategyCommunity::NAME, [](LogicalCollection* collection) { 
    return std::make_unique<ShardingStrategyCommunity>(collection);
  });
#ifdef USE_ENTERPRISE
  registerFactory(ShardingStrategyEnterprise::NAME, [](LogicalCollection* collection) { 
    return std::make_unique<ShardingStrategyEnterprise>(collection);
  });
#endif
}
  
void ShardingFeature::registerFactory(std::string const& name, 
                                      ShardingStrategy::FactoryFunction const& func) {
  LOG_TOPIC(TRACE, Logger::CLUSTER) << "registering sharding strategy '" << name << "'";

  if (!_factories.emplace(name, func).second) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::string("sharding factory function '") + name + "' already registered");
  }
}

std::unique_ptr<ShardingStrategy> ShardingFeature::create(std::string const& name, LogicalCollection* collection) {
  auto it = _factories.find(name);
  
  if (it == _factories.end()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER, std::string("unknown sharding factory function '") + name + "'");
  }

  return (*it).second(collection);
}
