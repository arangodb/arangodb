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

#include <Agency/TransactionBuilder.h>
#include <Replication2/ReplicatedLog/AgencyLogSpecification.h>
#include <Replication2/ReplicatedState/AgencySpecification.h>

#include <memory>
#include "Replication2/ReplicatedState/StateCommon.h"

namespace arangodb::replication2::replicated_state {
struct Action {
  enum class ActionType {
    EmptyAction,
    AddStateToPlanAction,
    AddParticipantAction,
    UnExcludeParticipantAction
  };
  virtual auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope = 0;

  virtual ActionType type() const = 0;
  virtual void toVelocyPack(VPackBuilder& builder) const = 0;
  virtual ~Action() = default;
};

auto to_string(Action::ActionType action) -> std::string_view;

struct EmptyAction : Action {
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;

  ActionType type() const override { return ActionType::EmptyAction; };
  void toVelocyPack(VPackBuilder& builder) const override;
};

struct AddStateToPlanAction : Action {
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;

  ActionType type() const override { return ActionType::AddStateToPlanAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  AddStateToPlanAction(
      arangodb::replication2::agency::LogTarget const& logTarget,
      agency::Plan const& statePlan)
      : logTarget{logTarget}, statePlan{statePlan} {};

  arangodb::replication2::agency::LogTarget logTarget;
  agency::Plan statePlan;
};

struct AddParticipantAction : Action {
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;

  ActionType type() const override { return ActionType::AddParticipantAction; };
  void toVelocyPack(VPackBuilder& builder) const override;

  AddParticipantAction(LogId const& log, ParticipantId const& participant,
                       StateGeneration const& generation)
      : log{log}, participant{participant}, generation{generation} {};

  LogId log;
  ParticipantId participant;
  StateGeneration generation;
};

struct UnExcludeParticipantAction : Action {
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;

  ActionType type() const override {
    return ActionType::UnExcludeParticipantAction;
  };
  void toVelocyPack(VPackBuilder& builder) const override;

  UnExcludeParticipantAction(LogId const& log, ParticipantId const& participant)
      : log{log}, participant{participant} {};

  LogId log;
  ParticipantId participant;
};

}  // namespace arangodb::replication2::replicated_state
