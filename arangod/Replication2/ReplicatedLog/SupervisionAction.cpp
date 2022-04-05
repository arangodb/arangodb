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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////
#include "SupervisionAction.h"

#include "Agency/TransactionBuilder.h"
#include "Replication2/ReplicatedLog/AgencyLogSpecification.h"
#include "velocypack/Builder.h"
#include "velocypack/velocypack-common.h"

#include "Agency/AgencyPaths.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;
namespace paths = arangodb::cluster::paths::aliases;

namespace arangodb::replication2::replicated_log {

auto to_string(Action const& action) -> std::string_view {
  return std::visit([](auto&& arg) { return arg.name; }, action);
}

void toVelocyPack(Action const& action, VPackBuilder& builder) {
  std::visit([&builder](auto&& arg) { serialize(builder, arg); }, action);
}

auto execute(Action const& action, DatabaseID const& dbName, LogId const& log,
             std::optional<LogPlanSpecification> const plan,
             std::optional<LogCurrent> const current,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto planPath =
      paths::plan()->replicatedLogs()->database(dbName)->log(log)->str();

  auto currentPath = paths::current()
                         ->replicatedLogs()
                         ->database(dbName)
                         ->log(log)
                         ->supervision()
                         ->str();

  if (std::holds_alternative<EmptyAction>(action)) {
    return envelope;
  }

  auto ctx = ActionContext{std::move(plan), std::move(current)};

  std::visit([&](auto& action) { action.execute(ctx); }, action);

  if (std::holds_alternative<EmptyAction>(action)) {
    return envelope;
  }

  return envelope
      .write()
      // this is here to trigger all waitForPlan, even if we only
      // update current.
      .inc(paths::plan()->version()->str())
      .cond(ctx.hasPlanModification(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx).emplace_object(
                  planPath, [&](VPackBuilder& builder) {
                    ctx.getPlan().toVelocyPack(builder);
                  });
            })
      .cond(ctx.hasCurrentModification(),
            [&](arangodb::agency::envelope::write_trx&& trx) {
              return std::move(trx)
                  .emplace_object(
                      currentPath,
                      [&](VPackBuilder& builder) {
                        ctx.getCurrent().supervision->toVelocyPack(builder);
                      })
                  .inc(paths::current()->version()->str());
            })
      .end();
}

}  // namespace arangodb::replication2::replicated_log
