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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ErrorCode.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedState/StateCommon.h"

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_state::agency {

struct ImplementationSpec {
  std::string type;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
      -> ImplementationSpec;
};

struct Properties {
  ImplementationSpec implementation;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Properties;
};

struct Plan {
  LogId id;
  StateGeneration generation;
  Properties properties;

  struct Participant {
    StateGeneration generation;

    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Participant;
  };

  std::unordered_map<ParticipantId, Participant> participants;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Plan;
};

struct Current {
  struct ParticipantStatus {
    StateGeneration generation;
    SnapshotInfo snapshot;  // TODO might become an array later?

    friend auto operator==(ParticipantStatus const&,
                           ParticipantStatus const&) noexcept -> bool = default;
    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice)
        -> ParticipantStatus;
  };

  std::unordered_map<ParticipantId, ParticipantStatus> participants;

  friend auto operator==(Current const&, Current const&) noexcept
      -> bool = default;
  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Current;
};

struct Target {
  LogId id;

  Properties properties;

  struct Participant {
    void toVelocyPack(velocypack::Builder& builder) const;
    [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Participant;
  };

  std::unordered_map<ParticipantId, Participant> participants;
  LogConfig config;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> Target;
};

struct State {
  Target target;
  std::optional<Plan> plan;
  std::optional<Current> current;
};

}  // namespace arangodb::replication2::replicated_state::agency
