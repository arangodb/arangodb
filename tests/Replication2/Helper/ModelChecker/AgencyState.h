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
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include <boost/container_hash/hash_fwd.hpp>
#include <optional>

namespace arangodb::test {

struct AgencyState {
  std::optional<arangodb::replication2::agency::Log> replicatedLog{};
  arangodb::replication2::replicated_log::ParticipantsHealth health;

  // FIXME: strictly speaking this is a hack, as it does not form part of the
  // agency's state; it is currently the simplest way to persist informatioon
  // for predicates to access;
  std::optional<size_t> logLeaderWriteConcern;
  std::optional<bool> logLeaderWaitForSync;

  friend std::size_t hash_value(AgencyState const& s) {
    std::size_t seed = 0;
    boost::hash_combine(seed, s.replicatedLog);
    boost::hash_combine(seed, s.health);
    return seed;
  }
  friend auto operator==(AgencyState const& s, AgencyState const& s2) noexcept
      -> bool = default;

  friend auto operator<<(std::ostream& os, AgencyState const& state)
      -> std::ostream& {
    auto const print = [&](auto const& x) {
      VPackBuilder builder;
      velocypack::serialize(builder, x);
      os << builder.toJson() << std::endl;
    };

    if (state.replicatedLog) {
      os << "Log/Target: ";
      print(state.replicatedLog->target);
      if (state.replicatedLog->plan) {
        os << "Log/Plan: ";
        print(*state.replicatedLog->plan);
      }
      if (state.replicatedLog->current) {
        os << "Log/Current: ";
        print(*state.replicatedLog->current);
      }
    }
    if (state.logLeaderWriteConcern) {
      os << "logLeaderWriteConcern: " << *state.logLeaderWriteConcern
         << std::endl;
    }
    if (state.logLeaderWaitForSync) {
      os << "logLeaderWaitForSync: " << *state.logLeaderWaitForSync
         << std::endl;
    }
    {
      VPackBuilder builder;
      {
        VPackObjectBuilder ob(&builder);
        for (auto const& [name, ph] : state.health._health) {
          builder.add(VPackValue(name));
          VPackObjectBuilder ob2(&builder);
          builder.add("rebootId", ph.rebootId.value());
          builder.add("failed", !ph.notIsFailed);
        }
      }
      os << builder.toString() << std::endl;
    }

    return os;
  }
};
}  // namespace arangodb::test
