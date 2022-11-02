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

namespace RLA = arangodb::replication2::agency;
namespace RSA = arangodb::replication2::replicated_state::agency;
namespace paths = arangodb::cluster::paths::aliases;

using namespace arangodb::replication2;

namespace arangodb::replication2::replicated_state {

auto executeAction(RSA::State state, std::optional<RLA::Log> log,
                   Action& action) -> ActionContext {
  auto logTarget = std::invoke([&]() -> std::optional<RLA::LogTarget> {
    if (log) {
      return std::move(log->target);
    }
    return std::nullopt;
  });

  auto statePlan = std::invoke([&]() -> std::optional<RSA::Plan> {
    if (state.plan) {
      return std::move(state.plan);
    }
    return std::nullopt;
  });

  auto currentSupervision =
      std::invoke([&]() -> std::optional<RSA::Current::Supervision> {
        if (state.current) {
          return std::move(state.current->supervision);
        }
        return RSA::Current::Supervision{};
      });

  auto actionCtx = replicated_state::ActionContext{
      std::move(logTarget), std::move(statePlan),
      std::move(currentSupervision)};
  std::visit([&](auto& action) { action.execute(actionCtx); }, action);
  return actionCtx;
}
}  // namespace arangodb::replication2::replicated_state
