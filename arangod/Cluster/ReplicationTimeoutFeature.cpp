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

#include "ReplicationTimeoutFeature.h"

#include "FeaturePhases/DatabaseFeaturePhase.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::options;

namespace arangodb {

double ReplicationTimeoutFeature::timeoutFactor = 1.0;
double ReplicationTimeoutFeature::timeoutPer4k = 0.1;
double ReplicationTimeoutFeature::lowerLimit = 30.0;  // longer than heartbeat timeout

ReplicationTimeoutFeature::ReplicationTimeoutFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ReplicationTimeout") {
  setOptional(true);
  startsAfter<application_features::DatabaseFeaturePhase>();
}

void ReplicationTimeoutFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("cluster", "Configure the cluster");

  options->addOption(
      "--cluster.synchronous-replication-timeout-minimum",
      "all synchronous replication timeouts will be at least this value (in seconds)",
      new DoubleParameter(&lowerLimit));

  options->addOption(
      "--cluster.synchronous-replication-timeout-factor",
      "all synchronous replication timeouts are multiplied by this factor",
      new DoubleParameter(&timeoutFactor));

  options->addOption(
      "--cluster.synchronous-replication-timeout-per-4k",
      "all synchronous replication timeouts are increased by this amount per "
      "4096 bytes (in seconds)",
      new DoubleParameter(&timeoutPer4k),
      arangodb::options::makeDefaultFlags(arangodb::options::Flags::Hidden));
}

}  // namespace arangodb
