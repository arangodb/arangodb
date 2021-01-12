////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
#include "RestServer/MetricsFeature.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"

using namespace arangodb::options;

namespace arangodb {

double ReplicationTimeoutFeature::timeoutFactor = 1.0;
double ReplicationTimeoutFeature::timeoutPer4k = 0.1;
double ReplicationTimeoutFeature::lowerLimit = 900.0;  // used to be 30.0
double ReplicationTimeoutFeature::upperLimit = 3600.0;  // used to be 120.0

// We essentially stop using a meaningful timeout for this operation. 
// This is achieved by setting the default for the minimal timeout to 1h or 3600s.
// The reason behind this is the following: We have to live with RocksDB stalls
// and write stops, which can happen in overload situations. Then, no meaningful
// timeout helps and it is almost certainly better to keep trying to not have
// to drop the follower and make matters worse. In case of an actual failure
// (or indeed a restart), the follower is marked as failed and its reboot id is
// increased. As a consequence, the connection is aborted and we run into an
// error anyway. This is when a follower will be dropped.

ReplicationTimeoutFeature::ReplicationTimeoutFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "ReplicationTimeout") {
  setOptional(true);
  startsAfter<application_features::DatabaseFeaturePhase>();
}

void ReplicationTimeoutFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("cluster", "Configure the cluster");

  options->addOption("--cluster.synchronous-replication-timeout-minimum",
                     "all synchronous replication timeouts will be at least "
                     "this value (in seconds)",
                     new DoubleParameter(&lowerLimit));

  options->addOption("--cluster.synchronous-replication-timeout-maximum",
                     "all synchronous replication timeouts will be at most "
                     "this value (in seconds)",
                     new DoubleParameter(&upperLimit))
                     .setIntroducedIn(30800);

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

void ReplicationTimeoutFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (upperLimit < lowerLimit) {
    LOG_TOPIC("8a9f3", WARN, Logger::CONFIG)
        << "--cluster.synchronous-replication-timeout-maximum must be at least "
        << "--cluster.synchronous-replication-timeout-minimum, setting max to "
        << "min";
    upperLimit = lowerLimit;
  }
}

}  // namespace arangodb
