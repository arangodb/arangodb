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

#include "ReplicationTimeoutFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Cluster/ReplicationTimeoutOptionsProvider.h"
#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "ProgramOptions/ProgramOptions.h"

using namespace arangodb::options;

namespace arangodb {

// We essentially stop using a meaningful timeout for this operation.
// This is achieved by setting the default for the minimal timeout to 1h or
// 3600s. The reason behind this is the following: We have to live with RocksDB
// stalls and write stops, which can happen in overload situations. Then, no
// meaningful timeout helps and it is almost certainly better to keep trying to
// not have to drop the follower and make matters worse. In case of an actual
// failure (or indeed a restart), the follower is marked as failed and its
// reboot id is increased. As a consequence, the connection is aborted and we
// run into an error anyway. This is when a follower will be dropped.

ReplicationTimeoutFeature::ReplicationTimeoutFeature(
    application_features::ApplicationServer& server)
    : application_features::ApplicationFeature{server, *this} {
  setOptional(true);
  startsAfter<application_features::DatabaseFeaturePhase>();
}

void ReplicationTimeoutFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  ReplicationTimeoutOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void ReplicationTimeoutFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  ReplicationTimeoutOptionsProvider provider;
  provider.validateOptions(options, _options);
}

}  // namespace arangodb
