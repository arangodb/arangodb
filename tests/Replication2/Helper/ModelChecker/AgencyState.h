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
#pragma once
#include <optional>
#include <boost/container_hash/hash_fwd.hpp>
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"

namespace arangodb::test {

struct AgencyState {
  std::optional<arangodb::replication2::replicated_state::agency::State>
      replicatedState;
  std::optional<arangodb::replication2::agency::Log> replicatedLog;
  arangodb::replication2::replicated_log::ParticipantsHealth health;

  friend std::size_t hash_value(AgencyState const& s) {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.replicatedState);
    boost::hash_combine(seed, s.replicatedLog);
    boost::hash_combine(seed, s.health);
    return seed;
  }
  friend auto operator==(AgencyState const& s, AgencyState const& s2) noexcept
      -> bool = default;

  friend auto operator<<(std::ostream& os, AgencyState const& state)
      -> std::ostream& {
    return os;
    auto const print = [&](auto const& x) {
      VPackBuilder builder;
      x.toVelocyPack(builder);
      os << builder.toJson() << std::endl;
    };

    print(state.replicatedState->target);
    if (state.replicatedState->plan) {
      print(*state.replicatedState->plan);
    }
    if (state.replicatedState->current) {
      print(*state.replicatedState->current);
    }
    if (state.replicatedLog) {
      print(state.replicatedLog->target);
      if (state.replicatedLog->plan) {
        print(*state.replicatedLog->plan);
      }
      if (state.replicatedLog->current) {
        print(*state.replicatedLog->current);
      }
    }
    for (auto const& [name, ph] : state.health._health) {
      std::cout << name << " reboot id = " << ph.rebootId.value()
                << " failed = " << !ph.notIsFailed;
    }
    return os;
  }
};
}  // namespace arangodb::test
