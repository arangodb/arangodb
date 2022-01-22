////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeaturePhase.h"

namespace arangodb {
namespace application_features {

class DatabaseFeaturePhase;
class ClusterFeature;
class MaintenanceFeature;
class ReplicationTimeoutFeature;
class ReplicatedLogFeature;
class V8PlatformFeature;

class ClusterFeaturePhase : public ApplicationFeaturePhase {
 public:
  static constexpr std::string_view name() noexcept { return "ClusterPhase"; }

  template<typename Server>
  explicit ClusterFeaturePhase(Server& server)
      : ApplicationFeaturePhase(
            server, Server::template id<ClusterFeaturePhase>(), name()) {
    setOptional(false);
    startsAfter<DatabaseFeaturePhase, Server>();

    startsAfter<ClusterFeature, Server>();
    startsAfter<MaintenanceFeature, Server>();
    startsAfter<ReplicationTimeoutFeature, Server>();
    startsAfter<ReplicatedLogFeature, Server>();

    // use before here since platform feature is in lib
    startsBefore<V8PlatformFeature, Server>();
  }
};

}  // namespace application_features
}  // namespace arangodb
