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
#include "Replication2/Helper/ModelChecker/AgencyState.h"
#include "Replication2/Helper/ModelChecker/AgencyTransitions.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/ParticipantsHealth.h"
#include "Replication2/ReplicatedLog/SupervisionAction.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/SupervisionAction.h"
#include <boost/container_hash/hash.hpp>

namespace arangodb::test {

struct EmptyInternalState {
  friend auto operator==(EmptyInternalState const& lhs,
                         EmptyInternalState const& rhs) noexcept -> bool {
    return true;
  }
  friend auto operator<<(std::ostream& os, EmptyInternalState const&) noexcept
      -> std::ostream& {
    return os;
  }
  friend auto hash_value(EmptyInternalState const& i) noexcept -> std::size_t {
    return 0;
  }
};

template<typename Derived, typename InternalStateType = EmptyInternalState>
struct ActorBase {
  using InternalState = InternalStateType;

  auto expand(AgencyState const& s, InternalState const& i)
      -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> {
    auto result =
        std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>>{};

    auto actions = reinterpret_cast<Derived const&>(*this).step(s);
    for (auto& action : actions) {
      auto newState = s;
      std::visit([&](auto& action) { action.apply(newState); }, action);
      result.emplace_back(std::move(action), std::move(newState),
                          InternalState{});
    }

    return result;
  }
};

struct OnceInternalState {
  bool wasTriggered = false;
  friend auto operator==(OnceInternalState const& lhs,
                         OnceInternalState const& rhs) noexcept
      -> bool = default;
  friend auto operator<<(std::ostream& os, OnceInternalState const& s) noexcept
      -> std::ostream& {
    return os << "was triggered = " << std::boolalpha << s.wasTriggered;
  }
  friend auto hash_value(OnceInternalState const& i) noexcept -> std::size_t {
    return boost::hash_value(i.wasTriggered);
  }
};

template<typename Derived>
struct OnceActorBase {
  using InternalState = OnceInternalState;

  auto expand(AgencyState const& s, InternalState const& i)
      -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>> {
    if (i.wasTriggered) {
      return {};
    }
    auto result =
        std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>>{};

    auto actions = reinterpret_cast<Derived const&>(*this).step(s);
    for (auto& action : actions) {
      auto newState = s;
      std::visit([&](auto& action) { action.apply(newState); }, action);
      result.emplace_back(std::move(action), std::move(newState),
                          InternalState{.wasTriggered = true});
    }

    return result;
  }
};

struct SupervisionActor : ActorBase<SupervisionActor> {
  static auto stepReplicatedState(AgencyState const& agency)
      -> std::optional<AgencyTransition>;

  static auto stepReplicatedLog(AgencyState const& agency)
      -> std::optional<AgencyTransition>;

  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;
};

struct DBServerActor : ActorBase<DBServerActor> {
  explicit DBServerActor(replication2::ParticipantId name);

  [[nodiscard]] auto stepReplicatedState(AgencyState const& agency) const
      -> std::optional<AgencyTransition>;

  auto stepReplicatedLogReportTerm(AgencyState const& agency) const
      -> std::optional<AgencyTransition>;

  auto stepReplicatedLogLeaderCommit(AgencyState const& agency) const
      -> std::optional<AgencyTransition>;

  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId name;
};

struct KillLeaderActor : ActorBase<KillLeaderActor> {
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;
};

struct KillServerActor : ActorBase<KillServerActor> {
  explicit KillServerActor(replication2::ParticipantId name);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId name;
};

struct KillAnyServerActor {
  struct InternalState {
    bool wasKilled = false;
    friend auto operator==(InternalState const& lhs,
                           InternalState const& rhs) noexcept -> bool = default;
    friend auto operator<<(std::ostream& os, InternalState const& s) noexcept
        -> std::ostream& {
      return os << "was killed = " << std::boolalpha << s.wasKilled;
    }
    friend auto hash_value(InternalState const& i) noexcept -> std::size_t {
      return boost::hash_value(i.wasKilled);
    }
  };

  auto expand(AgencyState const& s, InternalState const& i)
      -> std::vector<std::tuple<AgencyTransition, AgencyState, InternalState>>;
};

struct AddServerActor : OnceActorBase<AddServerActor> {
  explicit AddServerActor(replication2::ParticipantId newServer);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId newServer;
};

struct RemoveServerActor : OnceActorBase<RemoveServerActor> {
  explicit RemoveServerActor(replication2::ParticipantId server);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId server;
};

struct ReplaceAnyServerActor : OnceActorBase<ReplaceAnyServerActor> {
  explicit ReplaceAnyServerActor(replication2::ParticipantId newServer);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId newServer;
};
struct ReplaceSpecificServerActor : OnceActorBase<ReplaceSpecificServerActor> {
  explicit ReplaceSpecificServerActor(replication2::ParticipantId oldServer,
                                      replication2::ParticipantId newServer);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId oldServer;
  replication2::ParticipantId newServer;
};

struct SetLeaderActor : OnceActorBase<SetLeaderActor> {
  explicit SetLeaderActor(replication2::ParticipantId newLeader);
  auto step(AgencyState const& agency) const -> std::vector<AgencyTransition>;

  replication2::ParticipantId newLeader;
};

}  // namespace arangodb::test
