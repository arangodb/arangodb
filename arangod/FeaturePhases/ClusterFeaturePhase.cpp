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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "ClusterFeaturePhase.h"

#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/MaintenanceFeature.h"
#include "Cluster/ReplicationTimeoutFeature.h"
#include "Replication2/ReplicatedLog/ReplicatedLogFeature.h"
#ifdef USE_V8
#include "V8/V8PlatformFeature.h"
#endif

namespace arangodb::application_features {

ClusterFeaturePhase::ClusterFeaturePhase(
    application_features::ApplicationServer& server)
    : ApplicationFeaturePhase{server, *this} {
  setOptional(false);
  startsAfter<DatabaseFeaturePhase>();

  startsAfter<ClusterFeature>();
  startsAfter<MaintenanceFeature>();
  startsAfter<ReplicationTimeoutFeature>();
  startsAfter<ReplicatedLogFeature>();

#ifdef USE_V8
  // use before here since platform feature is in lib
  startsBefore<V8PlatformFeature>();
#endif
}

}  // namespace arangodb::application_features
