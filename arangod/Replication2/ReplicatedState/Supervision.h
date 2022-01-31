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

namespace arangodb::replication2::replicated_state {

struct Action {
  enum class ActionType {
    EmptyAction,
  };
  virtual auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope = 0;

  virtual ActionType type() const = 0;
  virtual void toVelocyPack(VPackBuilder& builder) const = 0;
  virtual ~Action() = default;
};

struct EmptyAction : Action {
  auto execute(std::string dbName, arangodb::agency::envelope envelope)
      -> arangodb::agency::envelope override;

  ActionType type() const override { return ActionType::EmptyAction; };
  void toVelocyPack(VPackBuilder& builder) const override{};
};

auto checkReplicatedState(
    std::optional<arangodb::replication2::agency::Log> const& log,
    agency::State const& state) -> std::unique_ptr<Action>;

}  // namespace arangodb::replication2::replicated_state
