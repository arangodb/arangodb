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
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterTtlMethods.h"
#include "Cluster/ServerState.h"
#include "RestServer/TtlFeature.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::methods;

Result Ttl::getStatistics(TtlFeature& feature, VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    TtlStatistics stats;
    auto& clusterFeature = feature.server().getFeature<ClusterFeature>();
    Result res = getTtlStatisticsFromAllDBServers(clusterFeature, stats);
    stats.toVelocyPack(out);
    return res;
  }

  feature.statsToVelocyPack(out);
  return Result();
}

Result Ttl::getProperties(TtlFeature& feature, VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    auto& clusterFeature = feature.server().getFeature<ClusterFeature>();
    return getTtlPropertiesFromAllDBServers(clusterFeature, out);
  }

  feature.propertiesToVelocyPack(out);
  return Result();
}

Result Ttl::setProperties(TtlFeature& feature, VPackSlice properties, VPackBuilder& out) {
  if (ServerState::instance()->isCoordinator()) {
    auto& clusterFeature = feature.server().getFeature<ClusterFeature>();
    return setTtlPropertiesOnAllDBServers(clusterFeature, properties, out);
  }

  return feature.propertiesFromVelocyPack(properties, out);
}
