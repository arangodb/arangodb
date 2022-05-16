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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ReplicationTimeoutFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Logger/LogTopic.h"
#include "Logger/LogMacros.h"
#include "ProgramOptions/Option.h"
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

ReplicationTimeoutFeature::ReplicationTimeoutFeature(Server& server)
    : ArangodFeature{server, *this},
      _timeoutFactor(1.0),
      _timeoutPer4k(0.1),
      _lowerLimit(900.0),
      _upperLimit(3600.0),
      _shardSynchronizationAttemptTimeout(20.0 * 60.0) {
  setOptional(true);
  startsAfter<application_features::DatabaseFeaturePhase>();
}

void ReplicationTimeoutFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  options->addOption("--cluster.synchronous-replication-timeout-minimum",
                     "all synchronous replication timeouts will be at least "
                     "this value (in seconds)",
                     new DoubleParameter(&_lowerLimit),
                     arangodb::options::makeDefaultFlags(
                         arangodb::options::Flags::Uncommon,
                         arangodb::options::Flags::OnDBServer));

  options
      ->addOption("--cluster.synchronous-replication-timeout-maximum",
                  "all synchronous replication timeouts will be at most "
                  "this value (in seconds)",
                  new DoubleParameter(&_upperLimit),
                  arangodb::options::makeDefaultFlags(
                      arangodb::options::Flags::Uncommon,
                      arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(30800);

  options->addOption(
      "--cluster.synchronous-replication-timeout-factor",
      "all synchronous replication timeouts are multiplied by this factor",
      new DoubleParameter(&_timeoutFactor),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::OnDBServer));

  options->addOption(
      "--cluster.synchronous-replication-timeout-per-4k",
      "all synchronous replication timeouts are increased by this amount per "
      "4096 bytes (in seconds)",
      new DoubleParameter(&_timeoutPer4k),
      arangodb::options::makeDefaultFlags(
          arangodb::options::Flags::Uncommon,
          arangodb::options::Flags::OnDBServer));

  options
      ->addOption(
          "--cluster.shard-synchronization-attempt-timeout",
          "timeout (in seconds) for every shard synchronization attempt. "
          "running into the timeout will not lead to a synchronization "
          "failure, but will continue the synchronization shortly after. "
          "setting a timeout can help to split the replication of large "
          "shards into smaller chunks and release snapshots on the "
          "leader earlier",
          new DoubleParameter(&_shardSynchronizationAttemptTimeout),
          arangodb::options::makeDefaultFlags(
              arangodb::options::Flags::Uncommon,
              arangodb::options::Flags::OnDBServer))
      .setIntroducedIn(30902);
}

void ReplicationTimeoutFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  if (_upperLimit < _lowerLimit) {
    LOG_TOPIC("8a9f3", WARN, Logger::CONFIG)
        << "--cluster.synchronous-replication-timeout-maximum must be at least "
        << "--cluster.synchronous-replication-timeout-minimum, setting max to "
        << "min";
    _upperLimit = _lowerLimit;
  }
}

}  // namespace arangodb
