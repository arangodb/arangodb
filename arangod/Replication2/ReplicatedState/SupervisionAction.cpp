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

#include "SupervisionAction.h"
#include "Agency/AgencyPaths.h"
#include "Agency/TransactionBuilder.h"
#include "Basics/StringUtils.h"
#include "velocypack/Builder.h"

namespace paths = arangodb::cluster::paths::aliases;

using namespace arangodb::replication2;

namespace {
template<typename A, typename = void>
struct action_has_update_log_target : std::false_type {};
template<typename A>
struct action_has_update_log_target<A,
                                    std::void_t<decltype(&A::updateLogTarget)>>
    : std::true_type {};
template<typename A>
inline constexpr auto action_has_update_log_target_v =
    action_has_update_log_target<A>::value;
template<typename A, typename = void>
struct action_has_update_state_plan : std::false_type {};
template<typename A>
struct action_has_update_state_plan<A,
                                    std::void_t<decltype(&A::updateStatePlan)>>
    : std::true_type {};
template<typename A>
inline constexpr auto action_has_update_state_plan_v =
    action_has_update_state_plan<A>::value;

static_assert(
    action_has_update_log_target_v<replicated_state::AddParticipantAction>);
}  // namespace

auto replicated_state::execute(
    LogId id, DatabaseID const& database, Action action,
    std::optional<agency::Plan> state,
    std::optional<replication2::agency::LogTarget> log,
    arangodb::agency::envelope envelope) -> arangodb::agency::envelope {
  auto logTargetPath =
      paths::target()->replicatedLogs()->database(database)->log(id)->str();
  auto statePlanPath =
      paths::plan()->replicatedStates()->database(database)->state(id)->str();

  return std::visit(
      [&]<typename A>(A& action) {
        if constexpr (!action_has_update_state_plan_v<A> &&
                      !action_has_update_log_target_v<A>) {
          static_assert(std::is_same_v<EmptyAction, A>);
          return std::move(envelope);
        } else {
          auto writes = envelope.write();
          if constexpr (action_has_update_log_target_v<A>) {
            auto newTarget =
                std::move(log).value_or(replication2::agency::LogTarget{});
            action.updateLogTarget(newTarget);
            writes = std::move(writes)
                         .emplace_object(logTargetPath,
                                         [&](VPackBuilder& builder) {
                                           newTarget.toVelocyPack(builder);
                                         })
                         .inc(paths::target()->version()->str());
          }
          if constexpr (action_has_update_state_plan_v<A>) {
            auto newState = std::move(state).value_or(agency::Plan{});
            action.updateStatePlan(newState);
            writes = std::move(writes)
                         .emplace_object(statePlanPath,
                                         [&](VPackBuilder& builder) {
                                           newState.toVelocyPack(builder);
                                         })
                         .inc(paths::plan()->version()->str());
          }
          return std::move(writes).end();
        }
      },
      action);
}
