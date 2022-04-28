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

#pragma once

#include "Basics/Common.h"
#include "RestServer/arangod.h"

#include <string_view>

namespace arangodb {

class ReplicationTimeoutFeature : public ArangodFeature {
 public:
  static const std::string_view name() noexcept { return "ReplicationTimeout"; }

  explicit ReplicationTimeoutFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;

  double timeoutFactor() const noexcept { return _timeoutFactor; }
  double timeoutPer4k() const noexcept { return _timeoutPer4k; }
  double lowerLimit() const noexcept { return _lowerLimit; }
  double upperLimit() const noexcept { return _upperLimit; }

  double shardSynchronizationAttemptTimeout() const noexcept {
    return _shardSynchronizationAttemptTimeout;
  }

 private:
  // these limits configure the timeouts for synchronous replication (all in
  // seconds)
  double _timeoutFactor;
  double _timeoutPer4k;
  // mininum wait time for sync replication (default: 900 seconds)
  double _lowerLimit;
  // maxinum wait time for sync replication (default: 3600 seconds)
  double _upperLimit;

  // timeout (in seconds) for shard synchronization attempts.
  // if the timeout is reached, this does will not count as a synchronization
  // failure. instead, the synchronization will be retried shortly after. the
  // rationale for setting a timeout here is that splitting the synchronization
  // of a large collection in one go would require the leader to keep its WAL
  // files for the synchronization snapshot for a very long time. breaking up
  // the synchronization into smaller pieces will allow the leader to release
  // the snapshots earlier, which mitigates potential WAL file pileups.
  double _shardSynchronizationAttemptTimeout;
};

}  // namespace arangodb
