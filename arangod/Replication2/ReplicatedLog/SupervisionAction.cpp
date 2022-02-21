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
#include "velocypack/velocypack-aliases.h"
#include "velocypack/velocypack-common.h"

#include "Agency/AgencyPaths.h"

#include "Logger/LogMacros.h"

using namespace arangodb::replication2::agency;
using namespace arangodb::cluster::paths;
namespace paths = arangodb::cluster::paths::aliases;

// helper type for the visitor #4
template <class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
// explicit deduction guide (not needed as of C++20)
template <class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

namespace arangodb::replication2::replicated_log {

auto to_string(Action const &action) -> std::string_view {
  return std::visit(overloaded{[](auto arg) { return arg.name; }}, action);
}

auto toVelocyPack(Action const &action, VPackBuilder &builder) {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(action)));
}

struct Context {
  explicit Context(LogId const &id, std::string const &dbName)
      : id{id}, dbName{dbName},
        targetRootPath{
            paths::target()->replicatedLogs()->database(dbName)->log(id)},
        planRootPath{
            paths::plan()->replicatedLogs()->database(dbname)->log(id)},
        currentRootPath{
            paths::current()->replicatedLogs()->database(dbname)->log(id)} {};
  LogId const &id;
  std::string const &dbName;

  std::shared_ptr<Path> const &targetRootPath;
  std::shared_ptr<Path> const &planRootPath;
  std::shared_ptr<Path> const &currentRootPath;
};

auto execute(AddLogToPlanAction const &action, Context const &ctx,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {

  auto const path = ctx.planRootPath->str();

  return envelope.write()
      .emplace_object(
          path,
          [&](VPackBuilder &builder) { action._spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
};

auto execute(AddParticipantsToTargetAction const &action,
             AgencyPath const &logRoot, arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path =
      paths::target()->replicatedLogs()->database(dbName)->log(_spec.id)->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder &builder) { _spec.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      //      .isEmpty(path)
      .end();
};

#if 0
auto CreateInitialTermAction::execute(std::string dbName,
                                      arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder &builder) { _term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path)
      .end();
}

auto DictateLeaderAction::execute(std::string dbName,
                                  arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder &builder) { _term.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())

      /* TODO: previous term should still be there
            .precs()
            .isEmpty(path)
       */
      .end();

  return envelope;
}

// These are really two atomic operations:
// update a participant's flags and cretae a new term.
auto makeEvictLeaderAction(ParticipantFlags flags, LogTerm term)
    -> EvictLeaderAction {
  flags.excluded = true;
  term.term = LogTerm{term.term.value + 1};
  term.leader.reset();
  // TODO: generation isn't handled but has to be

  return EvictLeaderAction{flags, term};
}

auto execute(EvictLeaderAction, std::string dbName,
             arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()->replicatedLogs()->database(dbName)->log(_id);

  return envelope.write()
      .emplace_object(
          path->participantsConfig()->participants()->server(_leader)->str(),
          [&](VPackBuilder &builder) { _flags.toVelocyPack(builder); })
      .emplace_object(
          path->currentTerm()->str(),
          [&](VPackBuilder &builder) { _newTerm.toVelocyPack(builder); })
      .inc(path->participantsConfig()->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(path->participantsConfig()->generation()->str(), _generation)
      .end();
}

/*
 * UpdateTermAction
 */
void UpdateTermAction::toVelocyPack(VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("newTerm"));
  _newTerm.toVelocyPack(builder);
}

auto UpdateTermAction::execute(std::string dbName,
                               arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->currentTerm()
                  ->str();

  return envelope.write()
      .emplace_object(
          path, [&](VPackBuilder &builder) { _newTerm.toVelocyPack(builder); })
      .inc(paths::plan()->version()->str())
      .end();
}

/*
 * LeaderElectionAction
 */
void LeaderElectionAction::toVelocyPack(VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add(VPackValue("campaign"));
  _election.toVelocyPack(builder);

  if (_newTerm) {
    builder.add(VPackValue("newTerm"));
    _newTerm->toVelocyPack(builder);
  }
}

auto LeaderElectionAction::execute(std::string dbName,
                                   arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto plan_path = paths::plan()
                       ->replicatedLogs()
                       ->database(dbName)
                       ->log(_id)
                       ->currentTerm()
                       ->str();
  auto current_path = paths::current()
                          ->replicatedLogs()
                          ->database(dbName)
                          ->log(_id)
                          ->supervision()
                          ->election()
                          ->str();

  if (_election.outcome == LogCurrentSupervisionElection::Outcome::SUCCESS) {
    TRI_ASSERT(_newTerm);
    return envelope.write()
        .emplace_object(
            plan_path,
            [&](VPackBuilder &builder) { _newTerm->toVelocyPack(builder); })
        .inc(paths::plan()->version()->str())
        .emplace_object(
            current_path,
            [&](VPackBuilder &builder) { _election.toVelocyPack(builder); })
        .inc(paths::current()->version()->str())
        .precs()
        .end();
  } else {
    return envelope.write()
        .emplace_object(
            current_path,
            [&](VPackBuilder &builder) { _election.toVelocyPack(builder); })
        .inc(paths::current()->version()->str())
        .precs()
        .end();
  }
}

/*
 * UpdateParticipantFlagsAction
 */
void UpdateParticipantFlagsAction::toVelocyPack(VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));

  builder.add("participant", VPackValue(_participant));
  builder.add(VPackValue("flags"));
  _flags.toVelocyPack(builder);
}
auto UpdateParticipantFlagsAction::execute(std::string dbName,
                                           arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .emplace_object(
          path->participants()->server(_participant)->str(),
          [&](VPackBuilder &builder) { _flags.toVelocyPack(builder); })
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * AddParticipantToPlanAction
 */
void AddParticipantToPlanAction::toVelocyPack(VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}
auto AddParticipantToPlanAction::execute(std::string dbName,
                                         arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .emplace_object(
          path->participants()->server(_participant)->str(),
          [&](VPackBuilder &builder) { _flags.toVelocyPack(builder); })
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isEmpty(path->participants()->server(_participant)->str())
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * RemoveParticipantFromPlanAction
 */
void RemoveParticipantFromPlanAction::toVelocyPack(
    VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}

auto RemoveParticipantFromPlanAction::execute(
    std::string dbName, arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  auto path = paths::plan()
                  ->replicatedLogs()
                  ->database(dbName)
                  ->log(_id)
                  ->participantsConfig();

  return envelope.write()
      .remove(path->participants()->server(_participant)->str())
      .inc(path->generation()->str())
      .inc(paths::plan()->version()->str())
      .precs()
      .isNotEmpty(path->participants()->server(_participant)->str())
      .isEqual(path->generation()->str(), _generation)
      .end();
}

/*
 * UpdateLogConfigAction
 */
void UpdateLogConfigAction::toVelocyPack(VPackBuilder &builder) const {
  auto ob = VPackObjectBuilder(&builder);
  builder.add(VPackValue("type"));
  builder.add(VPackValue(to_string(type())));
}
auto UpdateLogConfigAction::execute(std::string dbName,
                                    arangodb::agency::envelope envelope)
    -> arangodb::agency::envelope {
  // It is currently undefined what should happen if someone changes
  // the configuration
  TRI_ASSERT(false);
  return envelope;
}
#endif

} // namespace arangodb::replication2::replicated_log
