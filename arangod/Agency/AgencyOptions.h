////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2025 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace arangodb {

struct AgencyOptions {
  bool activated = false;
  uint64_t size = 1;
  uint64_t poolSize = 1;
  double minElectionTimeout = 1.0;
  double maxElectionTimeout = 5.0;
  bool supervision = false;
  bool supervisionTouched = false;
  bool waitForSync = true;
  double supervisionFrequency = 1.0;
  uint64_t compactionStepSize = 1000;
  uint64_t compactionKeepSize = 50000;
  uint64_t maxAppendSize = 250;
  double supervisionGracePeriod = 10.0;
  double supervisionOkThreshold = 5.0;
  double supervisionExpiredServersGracePeriod = 3600.0;
  uint64_t supervisionDelayAddFollower = 0;
  uint64_t supervisionDelayFailedFollower = 0;
  bool failedLeaderAddsFollower = true;
  std::string agencyMyAddress;
  std::vector<std::string> agencyEndpoints;
  std::string recoveryId;
};

}  // namespace arangodb
