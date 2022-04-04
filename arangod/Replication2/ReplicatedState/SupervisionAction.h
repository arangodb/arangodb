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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <memory>
#include <utility>

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "Replication2/ReplicatedState/AgencySpecification.h"
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {

template<typename T>
struct ModifySomeType {
  explicit ModifySomeType(std::optional<T> const& value) : value(value) {}
  template<typename F>
  auto modifyMyValue(F&& fn) {
    static_assert(std::is_invocable_r_v<void, F, T&>);
    TRI_ASSERT(value.has_value())
        << "modifying action expects value to be present";
    wasModified = true;
    return std::invoke(std::forward<F>(fn), *value);
  }

  template<typename... Vs>
  void setMyValue(Vs&&... args) {
    value.emplace(std::forward<Vs>(args)...);
    wasModified = true;
  }

  auto getMyValue() const -> T const& { return value.value(); }

  std::optional<T> value;
  bool wasModified = false;
};

template<typename... Ts>
struct ModifyContext : private ModifySomeType<Ts>... {
  explicit ModifyContext(std::optional<Ts>... values)
      : ModifySomeType<Ts>(std::move(values))... {}

  [[nodiscard]] auto hasModification() const noexcept -> bool {
    return (forType<Ts>().wasModified || ...);
  }

  template<typename... T, typename F>
  auto modify(F&& fn) {
    static_assert(std::is_invocable_v<F, T&...>);
    TRI_ASSERT((forType<T>().value.has_value() && ...))
        << "modifying action expects value to be present";
    ((forType<T>().wasModified = true), ...);
    return std::invoke(std::forward<F>(fn), (*forType<T>().value)...);
  }

  template<typename... T, typename F>
  auto modifyOrCreate(F&& fn) {
    static_assert(std::is_invocable_v<F, T&...>);
    (
        [&] {
          if (!forType<T>().value.has_value()) {
            static_assert(std::is_default_constructible_v<T>);
            forType<T>().value.emplace();
          }
        }(),
        ...);
    ((forType<T>().wasModified = true), ...);
    return std::invoke(std::forward<F>(fn), (*forType<T>().value)...);
  }

  template<typename T, typename... Args>
  auto setValue(Args&&... args) {
    return forType<T>().setMyValue(std::forward<Args>(args)...);
  }

  template<typename T>
  auto getValue() const -> T const& {
    return forType<T>().getMyValue();
  }

  template<typename T>
  [[nodiscard]] auto hasModificationFor() const noexcept -> bool {
    return forType<T>().wasModified;
  }

 private:
  template<typename T>
  auto forType() -> ModifySomeType<T>& {
    return static_cast<ModifySomeType<T>&>(*this);
  }
  template<typename T>
  auto forType() const -> ModifySomeType<T> const& {
    return static_cast<ModifySomeType<T> const&>(*this);
  }
};

using ActionContext = ModifyContext<replication2::agency::LogTarget,
                                    agency::Plan, agency::Current::Supervision>;
struct EmptyAction {
  void execute(ActionContext&) {}
};

struct AddParticipantAction {
  ParticipantId participant;
  StateGeneration generation;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          logTarget.participants[participant] = ParticipantFlags{
              .allowedInQuorum = false, .allowedAsLeader = false};

          plan.participants[participant].generation = plan.generation;
          plan.generation.value += 1;
        });
  }
};

struct RemoveParticipantFromLogTargetAction {
  ParticipantId participant;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          logTarget.participants.erase(participant);
        });
  }
};

struct RemoveParticipantFromStatePlanAction {
  ParticipantId participant;

  void execute(ActionContext& ctx) {
    ctx.modify<agency::Plan, replication2::agency::LogTarget>(
        [&](auto& plan, auto& logTarget) {
          plan.participants.erase(participant);
        });
  }
};

struct AddStateToPlanAction {
  replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;

  void execute(ActionContext& ctx) {
    ctx.setValue<agency::Plan>(std::move(statePlan));
    ctx.setValue<replication2::agency::LogTarget>(std::move(logTarget));
  }
};

struct UpdateParticipantFlagsAction {
  ParticipantId participant;
  ParticipantFlags flags;

  void execute(ActionContext& ctx) {
    ctx.modify<replication2::agency::LogTarget>(
        [&](auto& target) { target.participants.at(participant) = flags; });
  }
};

struct CurrentConvergedAction {
  std::uint64_t version;

  void execute(ActionContext& ctx) {
    ctx.modifyOrCreate<replicated_state::agency::Current::Supervision>(
        [&](auto& current) { current.version = version; });
  }
};

struct SetLeaderAction {
  std::optional<ParticipantId> leader;

  void execute(ActionContext& ctx) {
    ctx.modify<replication2::agency::LogTarget>(
        [&](replication2::agency::LogTarget& target) {
          target.leader = leader;
        });
  }
};

using Action = std::variant<
    EmptyAction, AddParticipantAction, RemoveParticipantFromLogTargetAction,
    RemoveParticipantFromStatePlanAction, AddStateToPlanAction,
    UpdateParticipantFlagsAction, CurrentConvergedAction, SetLeaderAction>;

auto execute(LogId id, DatabaseID const& database, Action action,
             std::optional<agency::Plan> state,
             std::optional<agency::Current::Supervision> currentSupervision,
             std::optional<replication2::agency::LogTarget> log,
             arangodb::agency::envelope envelope) -> arangodb::agency::envelope;

}  // namespace arangodb::replication2::replicated_state
