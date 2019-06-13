////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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

#include "Ttl.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/Common.h"
#include "Cluster/ClusterMethods.h"
#include "Cluster/ServerState.h"
#include "RestServer/TtlFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

Result Ttl::getStatistics(VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    TtlStatistics stats;
    Result res = getTtlStatisticsFromAllDBServers(stats);
    stats.toVelocyPack(out);
    return res;
  } 

  auto ttlFeature = arangodb::application_features::ApplicationServer::getFeature<TtlFeature>("Ttl");
  ttlFeature->statsToVelocyPack(out);
  return Result();
}

Result Ttl::getProperties(VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    return getTtlPropertiesFromAllDBServers(out);
  } 

  auto ttlFeature = arangodb::application_features::ApplicationServer::getFeature<TtlFeature>("Ttl");
  ttlFeature->propertiesToVelocyPack(out);
  return Result();
}

Result Ttl::setProperties(VPackSlice properties, VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    return setTtlPropertiesOnAllDBServers(properties, out);
  } 

  auto ttlFeature = arangodb::application_features::ApplicationServer::getFeature<TtlFeature>("Ttl");
  return ttlFeature->propertiesFromVelocyPack(properties, out);
}
