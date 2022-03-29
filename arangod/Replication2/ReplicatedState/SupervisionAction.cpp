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

auto replicated_state::execute(
    LogId id, DatabaseID const& database, Action action,
    std::optional<agency::Plan> state,
    std::optional<agency::Current::Supervision> currentSupervision,
    std::optional<replication2::agency::LogTarget> log,
    arangodb::agency::envelope envelope) -> arangodb::agency::envelope {
  auto logTargetPath =
      paths::target()->replicatedLogs()->database(database)->log(id)->str();
  auto statePlanPath =
      paths::plan()->replicatedStates()->database(database)->state(id)->str();
  auto currentSupervisionPath = paths::current()
                                    ->replicatedStates()
                                    ->database(database)
                                    ->state(id)
                                    ->supervision()
                                    ->str();

  if (std::holds_alternative<EmptyAction>(action)) {
    return envelope;
  }

  auto ctx = replicated_state::ActionContext{std::move(log), std::move(state),
                                             std::move(currentSupervision)};
  std::visit([&](auto& action) { action.execute(ctx); }, action);
  if (!ctx.hasModification()) {
    return envelope;
  }

  return envelope.write()
      .cond(ctx.hasModificationFor<replication2::agency::LogTarget>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      logTargetPath,
                      [&](VPackBuilder& builder) {
                        ctx.getValue<replication2::agency::LogTarget>()
                            .toVelocyPack(builder);
                      })
                  .inc(paths::target()->version()->str());
            })
      .cond(ctx.hasModificationFor<agency::Plan>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      statePlanPath,
                      [&](VPackBuilder& builder) {
                        ctx.getValue<agency::Plan>().toVelocyPack(builder);
                      })
                  .inc(paths::plan()->version()->str());
            })
      .cond(ctx.hasModificationFor<agency::Current::Supervision>(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(currentSupervisionPath,
                                  [&](VPackBuilder& builder) {
                                    ctx.getValue<agency::Current::Supervision>()
                                        .toVelocyPack(builder);
                                  })
                  .inc(paths::plan()->version()->str());
            })
      .end();
}
