////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "MaintenanceMock.h"

using namespace arangodb;
using namespace arangodb::test;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_state;
namespace RSA = arangodb::replication2::replicated_state::agency;

auto test::runReplicatedStateMaintenance(RSA::Plan const& plan,
                                         RSA::Current const& current)
    -> std::vector<RSA::Current> {
  std::vector<RSA::Current> result;
  /*
   * Each server in Plan that has not yet successfully transferred a snapshot
   * can make progress. Or none of them.
   */

  // find all active servers
  std::vector<std::pair<ParticipantId, StateGeneration>> activeServers;
  activeServers.reserve(plan.participants.size());
  for (auto const& [id, p] : plan.participants) {
    auto iter = current.participants.find(id);
    if (iter != current.participants.end()) {
      if (iter->second.generation == plan.generation &&
          iter->second.snapshot.status == SnapshotStatus::kCompleted) {
        continue;
      }
    }
    activeServers.emplace_back(id, p.generation);
  }

  result.reserve(1 << activeServers.size());
  for (std::size_t i = 0; i < result.size(); ++i) {
    auto newCurrent = current;
    for (std::size_t k = 0; k < activeServers.size(); ++k) {
      auto& [id, generation] = activeServers[k];
      auto serverCompleted = (i & (1 << k)) == 1;
      if (serverCompleted) {
        auto& status = newCurrent.participants[id];
        status.generation = generation;
        status.snapshot.status = SnapshotStatus::kCompleted;
      }
    }
    result.emplace_back(std::move(newCurrent));
  }

  return result;
}
